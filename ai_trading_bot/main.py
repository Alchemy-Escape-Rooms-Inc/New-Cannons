#!/usr/bin/env python3
"""
AI Trading Bot - Main Entry Point

A multifaceted AI-powered trading bot with machine learning, backtesting,
news sentiment analysis, and self-learning capabilities.
"""

import argparse
import sys
from pathlib import Path

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from ai_trading_bot.core.trading_bot import TradingBot
from ai_trading_bot.core.personality_profiles import list_profiles
from ai_trading_bot.backtesting import BacktestEngine, StrategyEvaluator
from ai_trading_bot.data import MarketDataCollector
from ai_trading_bot.utils.logger import get_logger
from ai_trading_bot.config import config

logger = get_logger(__name__)


def run_trading_bot(args):
    """Run the live/paper trading bot."""
    bot = TradingBot(
        initial_capital=args.capital,
        personality=args.personality,
        mode=args.mode
    )

    logger.info(f"Starting trading bot in {args.mode} mode")
    bot.start()


def run_backtest(args):
    """Run backtesting."""
    logger.info("Starting backtest mode")

    market_data = MarketDataCollector()
    evaluator = StrategyEvaluator()

    # Get historical data
    df = market_data.get_historical_data(
        args.symbol,
        start_date=args.start_date,
        end_date=args.end_date
    )

    if df.empty:
        logger.error(f"No data available for {args.symbol}")
        return

    # Run evaluation
    results = evaluator.evaluate_all_strategies(df)

    # Print results
    print("\n" + "=" * 80)
    print("BACKTEST RESULTS")
    print("=" * 80)
    print(results[['strategy_name', 'total_return', 'sharpe_ratio', 'win_rate', 'max_drawdown']].to_string())
    print("=" * 80)

    # Save results
    if args.output:
        results.to_csv(args.output, index=False)
        logger.info(f"Results saved to {args.output}")


def show_profiles(args):
    """Show available personality profiles."""
    from ai_trading_bot.core.personality_profiles import PERSONALITY_PROFILES

    print("\n" + "=" * 80)
    print("AVAILABLE PERSONALITY PROFILES")
    print("=" * 80 + "\n")

    for name, profile in PERSONALITY_PROFILES.items():
        print(f"{name}")
        print(f"  Description: {profile.description}")
        print(f"  Risk Tolerance: {profile.risk_tolerance}")
        print(f"  Trading Style: {profile.trading_style}")
        print(f"  Preferred Strategies: {', '.join(profile.preferred_strategies)}")
        print()

    print("=" * 80 + "\n")


def show_config(args):
    """Show current configuration."""
    import yaml

    print("\n" + "=" * 80)
    print("CURRENT CONFIGURATION")
    print("=" * 80 + "\n")

    print(yaml.dump(config.raw, default_flow_style=False))
    print("=" * 80 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description="AI Trading Bot - Multifaceted automated trading system",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run paper trading with balanced growth personality
  python main.py trade --mode paper --personality balanced_growth

  # Run backtesting on SPY
  python main.py backtest --symbol SPY --start-date 2020-01-01 --end-date 2023-12-31

  # Show available personality profiles
  python main.py profiles

  # Show current configuration
  python main.py config
        """
    )

    subparsers = parser.add_subparsers(dest='command', help='Command to run')

    # Trade command
    trade_parser = subparsers.add_parser('trade', help='Run the trading bot')
    trade_parser.add_argument(
        '--mode',
        choices=['paper', 'live'],
        default='paper',
        help='Trading mode (default: paper)'
    )
    trade_parser.add_argument(
        '--capital',
        type=float,
        default=100000.0,
        help='Initial capital (default: 100000)'
    )
    trade_parser.add_argument(
        '--personality',
        choices=list_profiles(),
        default='balanced_growth',
        help='Trading personality profile (default: balanced_growth)'
    )

    # Backtest command
    backtest_parser = subparsers.add_parser('backtest', help='Run backtesting')
    backtest_parser.add_argument(
        '--symbol',
        default='SPY',
        help='Symbol to backtest (default: SPY)'
    )
    backtest_parser.add_argument(
        '--start-date',
        default='2020-01-01',
        help='Start date (default: 2020-01-01)'
    )
    backtest_parser.add_argument(
        '--end-date',
        default='2023-12-31',
        help='End date (default: 2023-12-31)'
    )
    backtest_parser.add_argument(
        '--output',
        help='Output file for results (optional)'
    )

    # Profiles command
    subparsers.add_parser('profiles', help='Show available personality profiles')

    # Config command
    subparsers.add_parser('config', help='Show current configuration')

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return

    # Route to appropriate handler
    if args.command == 'trade':
        run_trading_bot(args)
    elif args.command == 'backtest':
        run_backtest(args)
    elif args.command == 'profiles':
        show_profiles(args)
    elif args.command == 'config':
        show_config(args)


if __name__ == '__main__':
    main()
