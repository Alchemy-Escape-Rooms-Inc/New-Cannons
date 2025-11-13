"""Risk management for trading operations."""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional
from datetime import datetime

from ..utils.logger import get_logger
from ..config import config

logger = get_logger(__name__)


class RiskManager:
    """Manages risk for trading operations."""

    def __init__(self, initial_capital: float = 100000.0):
        self.initial_capital = initial_capital
        self.max_position_size = config.get('trading.max_position_size', 0.1)
        self.max_portfolio_risk = config.get('trading.max_portfolio_risk', 0.02)
        self.max_daily_loss = config.get('trading.max_daily_loss', 0.05)
        self.max_correlation = config.get('risk.position_limits.max_correlation', 0.7)

        self.daily_pl = 0.0
        self.daily_start_capital = initial_capital

        logger.info("Risk Manager initialized")

    def reset_daily(self, current_capital: float):
        """Reset daily tracking."""
        self.daily_pl = 0.0
        self.daily_start_capital = current_capital
        logger.info(f"Daily reset - Starting capital: ${current_capital:.2f}")

    def check_daily_loss_limit(self) -> bool:
        """Check if daily loss limit has been hit."""
        daily_loss_pct = abs(self.daily_pl / self.daily_start_capital)

        if self.daily_pl < 0 and daily_loss_pct >= self.max_daily_loss:
            logger.warning(f"Daily loss limit hit: {daily_loss_pct*100:.2f}%")
            return False

        return True

    def calculate_position_size(
        self,
        capital: float,
        price: float,
        confidence: float = 1.0,
        volatility: float = None
    ) -> float:
        """Calculate position size based on risk parameters."""

        # Base position size as percentage of capital
        base_size = capital * self.max_position_size

        # Adjust for confidence
        adjusted_size = base_size * confidence

        # Adjust for volatility if available
        if volatility is not None:
            # Lower size in high volatility
            vol_adjustment = min(1.0, 0.02 / (volatility + 0.001))
            adjusted_size *= vol_adjustment

        # Convert to quantity
        quantity = adjusted_size / price

        logger.debug(f"Position size: {quantity:.2f} shares (${adjusted_size:.2f})")

        return quantity

    def kelly_criterion(
        self,
        win_rate: float,
        avg_win: float,
        avg_loss: float,
        capital: float,
        fraction: float = 0.25
    ) -> float:
        """Calculate position size using Kelly Criterion."""

        if avg_loss == 0 or win_rate == 0:
            return capital * self.max_position_size

        win_loss_ratio = abs(avg_win / avg_loss)

        # Kelly percentage
        kelly_pct = (win_rate * win_loss_ratio - (1 - win_rate)) / win_loss_ratio

        # Use fractional Kelly for safety
        kelly_pct = max(0, kelly_pct) * fraction

        # Cap at max position size
        kelly_pct = min(kelly_pct, self.max_position_size)

        position_size = capital * kelly_pct

        logger.debug(f"Kelly Criterion size: ${position_size:.2f} ({kelly_pct*100:.2f}%)")

        return position_size

    def calculate_stop_loss(
        self,
        entry_price: float,
        atr: float = None,
        method: str = 'fixed'
    ) -> float:
        """Calculate stop loss price."""

        if method == 'fixed':
            stop_pct = config.get('risk.stop_loss.default_pct', 0.02)
            stop_price = entry_price * (1 - stop_pct)

        elif method == 'trailing':
            stop_pct = config.get('risk.stop_loss.trailing_pct', 0.015)
            stop_price = entry_price * (1 - stop_pct)

        elif method == 'atr' and atr is not None:
            # ATR-based stop loss (typically 2-3x ATR)
            atr_multiplier = 2.0
            stop_price = entry_price - (atr * atr_multiplier)

        else:
            # Default
            stop_price = entry_price * 0.98

        return stop_price

    def calculate_take_profit(
        self,
        entry_price: float,
        method: str = 'fixed'
    ) -> list:
        """Calculate take profit levels."""

        if method == 'fixed':
            targets = config.get('risk.take_profit.targets', [0.02, 0.05, 0.10])
            return [entry_price * (1 + target) for target in targets]

        elif method == 'dynamic':
            # Dynamic based on market conditions
            return [
                entry_price * 1.02,  # 2%
                entry_price * 1.05,  # 5%
                entry_price * 1.10   # 10%
            ]

        elif method == 'ladder':
            # Ladder out at multiple levels
            return [
                entry_price * 1.015,  # 1.5%
                entry_price * 1.03,   # 3%
                entry_price * 1.05,   # 5%
                entry_price * 1.08    # 8%
            ]

        return [entry_price * 1.05]

    def check_correlation_limit(
        self,
        existing_symbols: list,
        new_symbol: str,
        correlation_matrix: pd.DataFrame
    ) -> bool:
        """Check if adding new position would violate correlation limits."""

        if not existing_symbols or correlation_matrix is None:
            return True

        # Check correlations with existing positions
        for symbol in existing_symbols:
            if symbol in correlation_matrix.index and new_symbol in correlation_matrix.columns:
                corr = abs(correlation_matrix.loc[symbol, new_symbol])

                if corr > self.max_correlation:
                    logger.warning(f"Correlation between {symbol} and {new_symbol} too high: {corr:.2f}")
                    return False

        return True

    def check_sector_allocation(
        self,
        current_allocations: Dict[str, float],
        new_symbol: str,
        new_allocation: float,
        symbol_sectors: Dict[str, str]
    ) -> bool:
        """Check if sector allocation limits are respected."""

        max_sector_allocation = config.get('portfolio.diversification.max_sector_allocation', 0.25)

        if new_symbol not in symbol_sectors:
            return True

        sector = symbol_sectors[new_symbol]

        # Calculate current sector allocation
        sector_allocation = sum(
            alloc for sym, alloc in current_allocations.items()
            if symbol_sectors.get(sym) == sector
        )

        # Add new allocation
        total_sector_allocation = sector_allocation + new_allocation

        if total_sector_allocation > max_sector_allocation:
            logger.warning(f"Sector {sector} allocation would exceed limit: {total_sector_allocation*100:.1f}%")
            return False

        return True

    def calculate_portfolio_var(
        self,
        positions: Dict[str, float],
        returns: pd.DataFrame,
        confidence: float = 0.95
    ) -> float:
        """Calculate portfolio Value at Risk (VaR)."""

        if returns.empty:
            return 0.0

        # Calculate portfolio returns
        portfolio_returns = returns[list(positions.keys())].dot(list(positions.values()))

        # Calculate VaR
        var = np.percentile(portfolio_returns, (1 - confidence) * 100)

        return abs(var)

    def should_reduce_risk(
        self,
        recent_performance: Dict[str, Any]
    ) -> bool:
        """Determine if risk should be reduced based on performance."""

        # Check consecutive losses
        consecutive_losses = recent_performance.get('consecutive_losses', 0)
        max_consecutive = config.get('risk.circuit_breakers.consecutive_losses', 5)

        if consecutive_losses >= max_consecutive:
            logger.warning(f"Consecutive losses limit reached: {consecutive_losses}")
            return True

        # Check drawdown
        current_drawdown = abs(recent_performance.get('current_drawdown', 0))
        max_drawdown = config.get('risk.circuit_breakers.drawdown_limit', 0.15)

        if current_drawdown >= max_drawdown:
            logger.warning(f"Drawdown limit reached: {current_drawdown*100:.1f}%")
            return True

        # Check daily loss
        if not self.check_daily_loss_limit():
            return True

        return False

    def update_daily_pl(self, trade_pl: float):
        """Update daily P&L tracker."""
        self.daily_pl += trade_pl

    def get_risk_adjusted_confidence(
        self,
        base_confidence: float,
        recent_performance: Dict[str, Any]
    ) -> float:
        """Adjust confidence based on recent risk metrics."""

        adjusted_confidence = base_confidence

        # Reduce confidence after losses
        if recent_performance.get('consecutive_losses', 0) > 2:
            adjusted_confidence *= 0.8

        # Reduce confidence in high drawdown
        drawdown = abs(recent_performance.get('current_drawdown', 0))
        if drawdown > 0.10:
            adjusted_confidence *= 0.7

        # Boost confidence after wins
        if recent_performance.get('consecutive_wins', 0) > 3:
            adjusted_confidence *= 1.1

        # Cap between 0 and 1
        adjusted_confidence = max(0.1, min(1.0, adjusted_confidence))

        return adjusted_confidence
