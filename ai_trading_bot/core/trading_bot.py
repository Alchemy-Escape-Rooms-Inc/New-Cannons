"""Main AI Trading Bot orchestrator - integrates all components."""

import pandas as pd
import numpy as np
from typing import Dict, List, Any, Optional
from datetime import datetime, time as dt_time
import schedule
import time

from ..ml_models import PredictionModel, EnsemblePredictor, ModelTrainer, FeatureEngineer
from ..backtesting import BacktestEngine, StrategyEvaluator
from ..data import MarketDataCollector, NewsCollector, SentimentAnalyzer
from ..portfolio import PortfolioManager, PositionTracker, RiskManager
from ..utils.logger import get_logger
from ..utils.database import Database
from ..config import config
from .personality_profiles import PersonalityProfile, get_profile

logger = get_logger(__name__)


class TradingBot:
    """AI Trading Bot - Main orchestrator integrating all components."""

    def __init__(
        self,
        initial_capital: float = 100000.0,
        personality: str = "balanced_growth",
        mode: str = "paper"
    ):
        logger.info("=" * 80)
        logger.info("INITIALIZING AI TRADING BOT")
        logger.info("=" * 80)

        self.initial_capital = initial_capital
        self.mode = mode  # paper or live
        self.personality = get_profile(personality)

        logger.info(f"Personality: {self.personality.name}")
        logger.info(f"Mode: {self.mode}")
        logger.info(f"Initial Capital: ${initial_capital:,.2f}")

        # Initialize components
        self.db = Database()
        self.market_data = MarketDataCollector()
        self.news_collector = NewsCollector()
        self.sentiment_analyzer = SentimentAnalyzer()
        self.portfolio = PortfolioManager(initial_capital)
        self.model_trainer = ModelTrainer()
        self.strategy_evaluator = StrategyEvaluator()
        self.feature_engineer = FeatureEngineer()

        # ML models
        self.prediction_models = {}
        self.current_strategy = None
        self.current_strategy_params = {}

        # Trading state
        self.is_trading = False
        self.market_open = False
        self.daily_trades_count = 0
        self.last_rebalance = None

        # Performance tracking
        self.daily_performance = {
            'trades': 0,
            'profit': 0.0,
            'loss': 0.0,
            'predictions_made': 0,
            'predictions_correct': 0
        }

        logger.info("AI Trading Bot initialized successfully")

    def start(self):
        """Start the trading bot."""
        logger.info("Starting AI Trading Bot...")

        # Initial setup
        self._run_startup_sequence()

        # Schedule daily tasks
        self._schedule_tasks()

        # Main trading loop
        self.is_trading = True

        logger.info("Trading Bot is now running")
        logger.info("Press Ctrl+C to stop")

        try:
            while self.is_trading:
                schedule.run_pending()
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("Stopping Trading Bot...")
            self.stop()

    def stop(self):
        """Stop the trading bot."""
        self.is_trading = False
        logger.info("Trading Bot stopped")

    def _schedule_tasks(self):
        """Schedule daily tasks."""
        # Pre-market analysis
        schedule.every().day.at("08:00").do(self.pre_market_analysis)

        # Market open preparation
        schedule.every().day.at("09:25").do(self.prepare_for_market_open)

        # Mid-day review
        schedule.every().day.at("12:00").do(self.mid_day_review)

        # End of day analysis
        schedule.every().day.at("16:05").do(self.end_of_day_analysis)

        # After hours learning
        schedule.every().day.at("17:00").do(self.after_hours_learning)

        # Weekly model retraining
        schedule.every().saturday.at("02:00").do(self.retrain_models)

        # Weekly review
        schedule.every().sunday.at("10:00").do(self.weekly_review)

    def _run_startup_sequence(self):
        """Run initial startup sequence."""
        logger.info("Running startup sequence...")

        # Load historical data
        symbols = config.get('data.universe.initial_stocks', ['SPY', 'QQQ'])
        logger.info(f"Loading data for {len(symbols)} symbols")

        # Screen for additional stocks
        screened = self.market_data.screen_stocks()
        symbols.extend(screened[:20])  # Add top 20 screened stocks

        # Train initial models
        logger.info("Training initial ML models...")
        for symbol in symbols[:5]:  # Train on first 5 for speed
            df = self.market_data.get_historical_data(symbol)
            if not df.empty:
                self.model_trainer.train_prediction_models(df)
                break

        # Evaluate strategies
        logger.info("Evaluating trading strategies...")
        if symbols:
            df = self.market_data.get_historical_data(symbols[0])
            if not df.empty:
                self.strategy_evaluator.evaluate_all_strategies(df)

        # Select initial strategy based on personality
        self.current_strategy = self.personality.preferred_strategies[0]
        logger.info(f"Selected initial strategy: {self.current_strategy}")

        logger.info("Startup sequence complete")

    def pre_market_analysis(self):
        """Pre-market analysis and planning."""
        logger.info("=" * 80)
        logger.info("PRE-MARKET ANALYSIS")
        logger.info("=" * 80)

        # Reset daily counters
        self.daily_trades_count = 0
        self.portfolio.risk_manager.reset_daily(self.portfolio.cash)
        self.daily_performance = {
            'trades': 0,
            'profit': 0.0,
            'loss': 0.0,
            'predictions_made': 0,
            'predictions_correct': 0
        }

        # Get market sentiment
        market_news = self.news_collector.get_market_news()
        market_sentiment = self.sentiment_analyzer.analyze_multiple_articles(market_news)

        logger.info(f"Market Sentiment: {market_sentiment['sentiment_label']} ({market_sentiment['overall_sentiment']:.3f})")

        # Analyze symbols in portfolio
        positions = self.portfolio.position_tracker.get_all_positions()

        for symbol in positions.keys():
            # Get news and sentiment
            news = self.news_collector.get_stock_news(symbol)
            if news:
                sentiment = self.sentiment_analyzer.analyze_multiple_articles(news)
                logger.info(f"{symbol} Sentiment: {sentiment['sentiment_label']} ({sentiment['overall_sentiment']:.3f})")

        # Make predictions for watchlist
        predictions = self._generate_daily_predictions()

        # Generate morning report
        self._generate_morning_report(predictions, market_sentiment)

        logger.info("Pre-market analysis complete")

    def _generate_daily_predictions(self) -> List[Dict[str, Any]]:
        """Generate predictions for potential trades."""
        predictions = []

        # Get watchlist
        symbols = self._get_watchlist()

        for symbol in symbols:
            try:
                # Get data
                df = self.market_data.get_historical_data(symbol)
                if df.empty:
                    continue

                # Engineer features
                df_featured = self.feature_engineer.engineer_features(df)

                # Get prediction from model (if trained)
                if 'price_predictor' in self.model_trainer.models:
                    ensemble = self.model_trainer.models['price_predictor']

                    # Prepare features
                    feature_cols = ensemble.models[0].feature_cols if ensemble.models else []
                    if feature_cols and all(col in df_featured.columns for col in feature_cols):
                        X = df_featured[feature_cols].iloc[-1:].values

                        # Make prediction
                        pred, confidence = ensemble.predict_with_confidence(X)

                        prediction = {
                            'symbol': symbol,
                            'predicted_return': pred[0],
                            'confidence': confidence[0],
                            'direction': 'up' if pred[0] > 0 else 'down',
                            'current_price': df['close'].iloc[-1],
                            'timestamp': datetime.now()
                        }

                        predictions.append(prediction)

                        # Store in database
                        self.db.store_prediction(
                            symbol=symbol,
                            prediction_type='price_prediction',
                            predicted_value=pred[0],
                            predicted_direction=prediction['direction'],
                            confidence=confidence[0],
                            model_version="ensemble_v1"
                        )

                        self.daily_performance['predictions_made'] += 1

            except Exception as e:
                logger.error(f"Error generating prediction for {symbol}: {e}")
                continue

        # Sort by confidence * predicted return
        predictions.sort(key=lambda x: abs(x['predicted_return']) * x['confidence'], reverse=True)

        return predictions

    def _get_watchlist(self) -> List[str]:
        """Get list of symbols to watch."""
        # Current positions
        symbols = list(self.portfolio.position_tracker.get_all_positions().keys())

        # Add screened stocks
        screened = self.market_data.screen_stocks()
        symbols.extend(screened[:30])

        # Remove duplicates
        return list(set(symbols))

    def _generate_morning_report(
        self,
        predictions: List[Dict[str, Any]],
        market_sentiment: Dict[str, Any]
    ):
        """Generate morning preview report."""
        logger.info("\n" + "=" * 80)
        logger.info("MORNING PREVIEW - " + datetime.now().strftime("%Y-%m-%d"))
        logger.info("=" * 80)

        # Portfolio summary
        current_prices = self._get_current_prices()
        portfolio_value = self.portfolio.get_portfolio_value(current_prices)
        total_return = (portfolio_value - self.initial_capital) / self.initial_capital * 100

        logger.info(f"\nPORTFOLIO SUMMARY")
        logger.info(f"  Total Value: ${portfolio_value:,.2f}")
        logger.info(f"  Cash: ${self.portfolio.cash:,.2f}")
        logger.info(f"  Total Return: {total_return:.2f}%")
        logger.info(f"  Open Positions: {len(self.portfolio.position_tracker.get_all_positions())}")

        # Market sentiment
        logger.info(f"\nMARKET SENTIMENT: {market_sentiment['sentiment_label'].upper()}")
        logger.info(f"  Score: {market_sentiment['overall_sentiment']:.3f}")
        logger.info(f"  Articles Analyzed: {market_sentiment['total_articles']}")

        # Top predictions
        logger.info(f"\nTOP PREDICTIONS FOR TODAY")
        for i, pred in enumerate(predictions[:5], 1):
            logger.info(f"  {i}. {pred['symbol']}: {pred['direction'].upper()} "
                       f"(Confidence: {pred['confidence']:.2f}, "
                       f"Expected Return: {pred['predicted_return']*100:.2f}%)")

        # Today's focus
        logger.info(f"\nTODAY'S FOCUS")
        logger.info(f"  Strategy: {self.current_strategy}")
        logger.info(f"  Max Trades: {self.personality.max_daily_trades}")
        logger.info(f"  Risk Level: {self.personality.risk_tolerance}")

        logger.info("=" * 80 + "\n")

    def prepare_for_market_open(self):
        """Prepare for market open."""
        logger.info("Preparing for market open...")
        self.market_open = True
        self.daily_trades_count = 0

    def mid_day_review(self):
        """Mid-day performance review."""
        logger.info("Running mid-day review...")

        current_prices = self._get_current_prices()
        portfolio_value = self.portfolio.get_portfolio_value(current_prices)

        logger.info(f"Mid-day Portfolio Value: ${portfolio_value:,.2f}")
        logger.info(f"Trades Today: {self.daily_trades_count}")
        logger.info(f"Daily P&L: ${self.portfolio.risk_manager.daily_pl:,.2f}")

    def end_of_day_analysis(self):
        """End of day analysis and reporting."""
        logger.info("=" * 80)
        logger.info("END OF DAY ANALYSIS")
        logger.info("=" * 80)

        self.market_open = False

        # Calculate daily performance
        current_prices = self._get_current_prices()
        portfolio_value = self.portfolio.get_portfolio_value(current_prices)
        daily_pl = self.portfolio.risk_manager.daily_pl

        # Get trades
        trades_today = self.db.get_trades_history(days=1)

        # Calculate metrics
        if not trades_today.empty:
            winning_trades = len(trades_today[trades_today['profit_loss'] > 0])
            losing_trades = len(trades_today[trades_today['profit_loss'] < 0])
            win_rate = winning_trades / len(trades_today) * 100 if len(trades_today) > 0 else 0
        else:
            winning_trades = losing_trades = 0
            win_rate = 0

        # Generate report
        logger.info(f"\nDAILY PERFORMANCE - {datetime.now().strftime('%Y-%m-%d')}")
        logger.info(f"  Portfolio Value: ${portfolio_value:,.2f}")
        logger.info(f"  Daily P&L: ${daily_pl:,.2f}")
        logger.info(f"  Total Trades: {len(trades_today) if not trades_today.empty else 0}")
        logger.info(f"  Winning Trades: {winning_trades}")
        logger.info(f"  Losing Trades: {losing_trades}")
        logger.info(f"  Win Rate: {win_rate:.1f}%")
        logger.info(f"  Predictions Made: {self.daily_performance['predictions_made']}")

        # Store performance
        self.db.store_performance_metrics(
            period='daily',
            metrics={
                'date': datetime.now().isoformat(),
                'portfolio_value': portfolio_value,
                'daily_pl': daily_pl,
                'total_trades': len(trades_today) if not trades_today.empty else 0,
                'winning_trades': winning_trades,
                'losing_trades': losing_trades,
                'win_rate': win_rate
            }
        )

        # Save portfolio snapshot
        self.portfolio.record_snapshot(current_prices)

        logger.info("=" * 80)

    def after_hours_learning(self):
        """After hours: learn from today's performance."""
        logger.info("Running after-hours learning...")

        # Evaluate predictions
        self._evaluate_todays_predictions()

        # Analyze what worked and what didn't
        trades_today = self.db.get_trades_history(days=1)

        if not trades_today.empty:
            # Analyze winning vs losing patterns
            winning = trades_today[trades_today['profit_loss'] > 0]
            losing = trades_today[trades_today['profit_loss'] < 0]

            if len(winning) > 0 and len(losing) > 0:
                # Compare patterns
                avg_win = winning['profit_loss'].mean()
                avg_loss = losing['profit_loss'].mean()

                logger.info(f"Average Win: ${avg_win:.2f}")
                logger.info(f"Average Loss: ${avg_loss:.2f}")

                # Log learning
                self.db.log_learning(
                    learning_type="daily_analysis",
                    description=f"Analyzed {len(trades_today)} trades",
                    previous_behavior="",
                    new_behavior=f"Win rate: {len(winning)/len(trades_today)*100:.1f}%",
                    trigger_event="end_of_day",
                    expected_improvement=0.0
                )

        logger.info("After-hours learning complete")

    def _evaluate_todays_predictions(self):
        """Evaluate accuracy of today's predictions."""
        from datetime import timedelta

        # Get predictions from earlier today that haven't been evaluated
        today_start = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)

        with self.db.get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("""
                SELECT id, symbol, predicted_value, predicted_direction, timestamp
                FROM predictions
                WHERE timestamp >= ? AND evaluation_timestamp IS NULL
                AND timestamp < datetime('now', '-1 hours')
            """, (today_start,))

            predictions = cursor.fetchall()

        if not predictions:
            return

        logger.info(f"Evaluating {len(predictions)} predictions from today")

        for pred in predictions:
            try:
                # Get historical data to see what actually happened
                df = self.market_data.get_historical_data(
                    pred['symbol'],
                    start_date=(datetime.fromisoformat(pred['timestamp']) - timedelta(days=1)).strftime('%Y-%m-%d'),
                    end_date=datetime.now().strftime('%Y-%m-%d')
                )

                if df.empty or len(df) < 2:
                    continue

                # Find the price at prediction time and current price
                pred_price = df.iloc[0]['close']  # Price at prediction time
                current_price = df.iloc[-1]['close']  # Current price

                # Calculate actual return
                actual_return = (current_price - pred_price) / pred_price
                actual_direction = 'up' if actual_return > 0 else 'down'

                # Evaluate prediction
                self.db.evaluate_prediction(
                    prediction_id=pred['id'],
                    actual_value=actual_return,
                    actual_direction=actual_direction,
                    profit_impact=0.0  # Would need to track if we traded on this
                )

                # Update daily performance
                if pred['predicted_direction'] == actual_direction:
                    self.daily_performance['predictions_correct'] += 1

            except Exception as e:
                logger.error(f"Error evaluating prediction {pred['id']}: {e}")
                continue

        accuracy_rate = (self.daily_performance['predictions_correct'] /
                         self.daily_performance['predictions_made'] * 100
                         if self.daily_performance['predictions_made'] > 0 else 0)

        logger.info(f"Prediction accuracy today: {accuracy_rate:.1f}%")

    def retrain_models(self):
        """Retrain ML models weekly."""
        logger.info("Retraining models...")

        symbols = self._get_watchlist()

        for symbol in symbols[:5]:  # Retrain on top 5
            df = self.market_data.get_historical_data(symbol)
            if not df.empty:
                self.model_trainer.retrain_models(df)

        logger.info("Model retraining complete")

    def weekly_review(self):
        """Weekly performance review."""
        logger.info("=" * 80)
        logger.info("WEEKLY REVIEW")
        logger.info("=" * 80)

        # Get week performance
        trades_week = self.db.get_trades_history(days=7)

        if not trades_week.empty:
            total_pl = trades_week['profit_loss'].sum()
            win_rate = len(trades_week[trades_week['profit_loss'] > 0]) / len(trades_week) * 100

            logger.info(f"Weekly P&L: ${total_pl:,.2f}")
            logger.info(f"Weekly Win Rate: {win_rate:.1f}%")
            logger.info(f"Total Trades: {len(trades_week)}")

        logger.info("=" * 80)

    def _get_current_prices(self) -> Dict[str, float]:
        """Get current prices for all positions."""
        prices = {}
        for symbol in self.portfolio.position_tracker.get_all_positions().keys():
            quote = self.market_data.get_real_time_quote(symbol)
            prices[symbol] = quote.get('price', 0)
        return prices
