"""Comprehensive backtesting engine."""

import numpy as np
import pandas as pd
from typing import Dict, List, Any, Optional, Tuple
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

from ..utils.logger import get_logger
from ..utils.database import Database
from ..config import config

logger = get_logger(__name__)


class Position:
    """Represents a trading position."""

    def __init__(
        self,
        symbol: str,
        quantity: float,
        entry_price: float,
        entry_time: datetime,
        side: str = 'long'
    ):
        self.symbol = symbol
        self.quantity = quantity
        self.entry_price = entry_price
        self.entry_time = entry_time
        self.side = side
        self.exit_price = None
        self.exit_time = None
        self.profit_loss = 0.0
        self.profit_loss_pct = 0.0

    def close(self, exit_price: float, exit_time: datetime):
        """Close the position."""
        self.exit_price = exit_price
        self.exit_time = exit_time

        if self.side == 'long':
            self.profit_loss = (exit_price - self.entry_price) * self.quantity
        else:  # short
            self.profit_loss = (self.entry_price - exit_price) * self.quantity

        self.profit_loss_pct = (self.profit_loss / (self.entry_price * self.quantity)) * 100

    @property
    def is_open(self) -> bool:
        return self.exit_price is None

    @property
    def holding_period(self) -> float:
        """Holding period in days."""
        if self.exit_time:
            return (self.exit_time - self.entry_time).total_seconds() / 86400
        return 0


class BacktestEngine:
    """Engine for backtesting trading strategies."""

    def __init__(
        self,
        initial_capital: float = 100000.0,
        commission: float = 0.001,
        slippage: float = 0.0005
    ):
        self.initial_capital = initial_capital
        self.capital = initial_capital
        self.commission = commission
        self.slippage = slippage

        self.positions: List[Position] = []
        self.open_positions: Dict[str, Position] = {}
        self.equity_curve = []
        self.trades = []

        self.db = Database()

    def reset(self):
        """Reset the backtest state."""
        self.capital = self.initial_capital
        self.positions = []
        self.open_positions = {}
        self.equity_curve = []
        self.trades = []

    def enter_position(
        self,
        symbol: str,
        quantity: float,
        price: float,
        timestamp: datetime,
        side: str = 'long'
    ) -> bool:
        """Enter a new position."""

        # Calculate costs
        position_cost = quantity * price
        commission_cost = position_cost * self.commission
        slippage_cost = position_cost * self.slippage
        total_cost = position_cost + commission_cost + slippage_cost

        # Check if we have enough capital
        if total_cost > self.capital:
            logger.warning(f"Insufficient capital for {symbol}: need ${total_cost:.2f}, have ${self.capital:.2f}")
            return False

        # Adjust price for slippage
        if side == 'long':
            entry_price = price * (1 + self.slippage)
        else:
            entry_price = price * (1 - self.slippage)

        # Create position
        position = Position(symbol, quantity, entry_price, timestamp, side)
        self.positions.append(position)
        self.open_positions[symbol] = position

        # Update capital
        self.capital -= total_cost

        logger.debug(f"Entered {side} position: {symbol} x{quantity} @ ${entry_price:.2f}")
        return True

    def exit_position(
        self,
        symbol: str,
        price: float,
        timestamp: datetime
    ) -> bool:
        """Exit an open position."""

        if symbol not in self.open_positions:
            return False

        position = self.open_positions[symbol]

        # Adjust price for slippage
        if position.side == 'long':
            exit_price = price * (1 - self.slippage)
        else:
            exit_price = price * (1 + self.slippage)

        # Close position
        position.close(exit_price, timestamp)

        # Calculate proceeds
        proceeds = position.quantity * exit_price
        commission_cost = proceeds * self.commission

        # Update capital
        self.capital += proceeds - commission_cost

        # Record trade
        self.trades.append({
            'symbol': symbol,
            'side': position.side,
            'quantity': position.quantity,
            'entry_price': position.entry_price,
            'exit_price': exit_price,
            'entry_time': position.entry_time,
            'exit_time': timestamp,
            'profit_loss': position.profit_loss,
            'profit_loss_pct': position.profit_loss_pct,
            'holding_period': position.holding_period
        })

        # Remove from open positions
        del self.open_positions[symbol]

        logger.debug(f"Exited position: {symbol} P&L: ${position.profit_loss:.2f} ({position.profit_loss_pct:.2f}%)")
        return True

    def get_portfolio_value(self, current_prices: Dict[str, float]) -> float:
        """Calculate current portfolio value."""

        # Cash
        portfolio_value = self.capital

        # Open positions
        for symbol, position in self.open_positions.items():
            if symbol in current_prices:
                current_value = position.quantity * current_prices[symbol]
                portfolio_value += current_value

        return portfolio_value

    def record_equity(self, timestamp: datetime, current_prices: Dict[str, float]):
        """Record current equity for equity curve."""

        portfolio_value = self.get_portfolio_value(current_prices)

        self.equity_curve.append({
            'timestamp': timestamp,
            'equity': portfolio_value,
            'cash': self.capital,
            'positions_value': portfolio_value - self.capital,
            'return': (portfolio_value / self.initial_capital - 1) * 100
        })

    def run_backtest(
        self,
        data: pd.DataFrame,
        strategy_func,
        strategy_params: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """Run a backtest with a given strategy."""

        self.reset()

        logger.info(f"Running backtest from {data.index[0]} to {data.index[-1]}")
        logger.info(f"Initial capital: ${self.initial_capital:.2f}")

        strategy_params = strategy_params or {}

        # Iterate through each time period
        for i in range(len(data)):
            current_date = data.index[i]
            current_data = data.iloc[:i+1]

            # Get current prices
            current_prices = {
                symbol: data['close'].iloc[i]
                for symbol in [data['symbol'].iloc[i]] if 'symbol' in data.columns
            }

            if not current_prices:
                current_prices = {'DEFAULT': data['close'].iloc[i]}

            # Call strategy function to get signals
            signals = strategy_func(current_data, self, strategy_params)

            # Process signals
            if signals:
                for signal in signals:
                    if signal['action'] == 'buy':
                        self.enter_position(
                            symbol=signal['symbol'],
                            quantity=signal['quantity'],
                            price=signal['price'],
                            timestamp=current_date,
                            side='long'
                        )
                    elif signal['action'] == 'sell':
                        self.exit_position(
                            symbol=signal['symbol'],
                            price=signal['price'],
                            timestamp=current_date
                        )

            # Record equity
            self.record_equity(current_date, current_prices)

        # Close any remaining open positions
        final_date = data.index[-1]
        final_prices = current_prices

        for symbol in list(self.open_positions.keys()):
            if symbol in final_prices:
                self.exit_position(symbol, final_prices[symbol], final_date)

        # Calculate performance metrics
        results = self.calculate_performance()

        logger.info(f"Backtest complete. Final capital: ${self.capital:.2f}")
        logger.info(f"Total return: {results['total_return']:.2f}%")
        logger.info(f"Sharpe ratio: {results['sharpe_ratio']:.2f}")
        logger.info(f"Max drawdown: {results['max_drawdown']:.2f}%")
        logger.info(f"Win rate: {results['win_rate']:.2f}%")

        return results

    def calculate_performance(self) -> Dict[str, Any]:
        """Calculate comprehensive performance metrics."""

        if not self.trades:
            return self._empty_results()

        trades_df = pd.DataFrame(self.trades)
        equity_df = pd.DataFrame(self.equity_curve)

        # Basic metrics
        final_capital = self.capital
        total_return = (final_capital / self.initial_capital - 1) * 100

        # Trade statistics
        total_trades = len(trades_df)
        winning_trades = len(trades_df[trades_df['profit_loss'] > 0])
        losing_trades = len(trades_df[trades_df['profit_loss'] < 0])
        win_rate = (winning_trades / total_trades * 100) if total_trades > 0 else 0

        # Profit metrics
        total_profit = trades_df[trades_df['profit_loss'] > 0]['profit_loss'].sum()
        total_loss = abs(trades_df[trades_df['profit_loss'] < 0]['profit_loss'].sum())
        net_profit = total_profit - total_loss
        profit_factor = (total_profit / total_loss) if total_loss > 0 else 0

        # Average metrics
        avg_win = trades_df[trades_df['profit_loss'] > 0]['profit_loss'].mean() if winning_trades > 0 else 0
        avg_loss = trades_df[trades_df['profit_loss'] < 0]['profit_loss'].mean() if losing_trades > 0 else 0
        avg_trade = trades_df['profit_loss'].mean()

        # Largest metrics
        largest_win = trades_df['profit_loss'].max() if total_trades > 0 else 0
        largest_loss = trades_df['profit_loss'].min() if total_trades > 0 else 0

        # Holding period
        avg_holding_period = trades_df['holding_period'].mean() if total_trades > 0 else 0

        # Risk-adjusted returns
        if len(equity_df) > 1:
            returns = equity_df['return'].pct_change().dropna()

            if len(returns) > 0 and returns.std() > 0:
                sharpe_ratio = (returns.mean() / returns.std()) * np.sqrt(252)
            else:
                sharpe_ratio = 0

            # Sortino ratio (downside deviation)
            downside_returns = returns[returns < 0]
            if len(downside_returns) > 0 and downside_returns.std() > 0:
                sortino_ratio = (returns.mean() / downside_returns.std()) * np.sqrt(252)
            else:
                sortino_ratio = 0

            # Maximum drawdown
            cumulative = (1 + returns).cumprod()
            running_max = cumulative.expanding().max()
            drawdown = (cumulative - running_max) / running_max
            max_drawdown = drawdown.min() * 100
        else:
            sharpe_ratio = 0
            sortino_ratio = 0
            max_drawdown = 0

        # Consecutive metrics
        consecutive_wins = 0
        consecutive_losses = 0
        max_consecutive_wins = 0
        max_consecutive_losses = 0

        for trade in self.trades:
            if trade['profit_loss'] > 0:
                consecutive_wins += 1
                consecutive_losses = 0
                max_consecutive_wins = max(max_consecutive_wins, consecutive_wins)
            else:
                consecutive_losses += 1
                consecutive_wins = 0
                max_consecutive_losses = max(max_consecutive_losses, consecutive_losses)

        return {
            'initial_capital': self.initial_capital,
            'final_capital': final_capital,
            'total_return': total_return,
            'net_profit': net_profit,
            'total_trades': total_trades,
            'winning_trades': winning_trades,
            'losing_trades': losing_trades,
            'win_rate': win_rate,
            'total_profit': total_profit,
            'total_loss': total_loss,
            'profit_factor': profit_factor,
            'avg_win': avg_win,
            'avg_loss': avg_loss,
            'avg_trade': avg_trade,
            'largest_win': largest_win,
            'largest_loss': largest_loss,
            'sharpe_ratio': sharpe_ratio,
            'sortino_ratio': sortino_ratio,
            'max_drawdown': max_drawdown,
            'avg_holding_period': avg_holding_period,
            'max_consecutive_wins': max_consecutive_wins,
            'max_consecutive_losses': max_consecutive_losses,
            'equity_curve': equity_df.to_dict('records'),
            'trades': trades_df.to_dict('records')
        }

    def _empty_results(self) -> Dict[str, Any]:
        """Return empty results structure."""
        return {
            'initial_capital': self.initial_capital,
            'final_capital': self.capital,
            'total_return': 0,
            'net_profit': 0,
            'total_trades': 0,
            'winning_trades': 0,
            'losing_trades': 0,
            'win_rate': 0,
            'sharpe_ratio': 0,
            'sortino_ratio': 0,
            'max_drawdown': 0
        }

    def compare_strategies(
        self,
        data: pd.DataFrame,
        strategies: Dict[str, tuple]
    ) -> pd.DataFrame:
        """Compare multiple strategies on the same data."""

        logger.info(f"Comparing {len(strategies)} strategies")

        results = []

        for strategy_name, (strategy_func, params) in strategies.items():
            logger.info(f"Testing strategy: {strategy_name}")

            perf = self.run_backtest(data, strategy_func, params)
            perf['strategy_name'] = strategy_name

            results.append(perf)

            # Store in database
            self.db.store_backtest_result(
                strategy_name=strategy_name,
                start_date=data.index[0],
                end_date=data.index[-1],
                initial_capital=perf['initial_capital'],
                final_capital=perf['final_capital'],
                total_return=perf['total_return'],
                sharpe_ratio=perf['sharpe_ratio'],
                max_drawdown=perf['max_drawdown'],
                win_rate=perf['win_rate'],
                total_trades=perf['total_trades'],
                parameters=str(params),
                results=str(perf)
            )

        # Convert to DataFrame for easy comparison
        comparison_df = pd.DataFrame(results)

        # Rank strategies
        comparison_df['sharpe_rank'] = comparison_df['sharpe_ratio'].rank(ascending=False)
        comparison_df['return_rank'] = comparison_df['total_return'].rank(ascending=False)
        comparison_df['drawdown_rank'] = comparison_df['max_drawdown'].abs().rank(ascending=True)

        comparison_df['overall_rank'] = (
            comparison_df['sharpe_rank'] +
            comparison_df['return_rank'] +
            comparison_df['drawdown_rank']
        ) / 3

        comparison_df = comparison_df.sort_values('overall_rank')

        logger.info("Strategy comparison complete")
        logger.info(f"\nTop 3 strategies:\n{comparison_df[['strategy_name', 'total_return', 'sharpe_ratio', 'max_drawdown']].head(3)}")

        return comparison_df

    def optimize_parameters(
        self,
        data: pd.DataFrame,
        strategy_func,
        param_grid: Dict[str, List[Any]],
        optimization_metric: str = 'sharpe_ratio'
    ) -> Tuple[Dict[str, Any], Dict[str, Any]]:
        """Optimize strategy parameters using grid search."""

        from itertools import product

        logger.info("Starting parameter optimization")

        # Generate all parameter combinations
        param_names = list(param_grid.keys())
        param_values = list(param_grid.values())
        param_combinations = list(product(*param_values))

        results = []

        for i, param_combo in enumerate(param_combinations):
            params = dict(zip(param_names, param_combo))

            logger.debug(f"Testing parameters {i+1}/{len(param_combinations)}: {params}")

            perf = self.run_backtest(data, strategy_func, params)
            perf['params'] = params

            results.append(perf)

        # Find best parameters
        results_df = pd.DataFrame(results)

        if optimization_metric in results_df.columns:
            best_idx = results_df[optimization_metric].idxmax()
            best_params = results_df.loc[best_idx, 'params']
            best_performance = results_df.loc[best_idx].to_dict()
        else:
            best_params = {}
            best_performance = {}

        logger.info(f"Optimization complete. Best {optimization_metric}: {best_performance.get(optimization_metric, 'N/A')}")
        logger.info(f"Best parameters: {best_params}")

        return best_params, best_performance
