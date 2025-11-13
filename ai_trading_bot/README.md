# AI Trading Bot

A comprehensive, multifaceted AI-powered automated trading bot with machine learning, backtesting, news sentiment analysis, and self-learning capabilities.

## 🎯 Core Features

### Machine Learning & Predictions
- **Ensemble ML Models**: XGBoost, LightGBM, Random Forest, Gradient Boosting
- **LSTM Neural Networks**: Deep learning for time series prediction
- **Self-Learning**: Continuously learns from prediction results and trading outcomes
- **Feature Engineering**: 50+ technical indicators and features
- **Adaptive Model Selection**: Automatically adjusts models based on performance

### Trading Strategies
- **Pre-Built Strategies**: 8 professional trading strategies
  - Momentum Trading
  - Mean Reversion
  - Trend Following
  - Breakout Trading
  - RSI Strategy
  - MACD Strategy
  - Pairs Trading
  - ML Hybrid Strategy
- **Strategy Backtesting**: Test strategies on historical data
- **Parameter Optimization**: Automatic parameter tuning
- **Walk-Forward Analysis**: Robust performance validation

### Portfolio Management
- **FIFO Position Tracking**: First-In-First-Out inventory management
- **Risk Management**: Stop-loss, take-profit, position sizing
- **Diversification**: Automatic sector and correlation limits
- **Rebalancing**: Automatic portfolio rebalancing
- **Kelly Criterion**: Optimal position sizing

### News & Sentiment Analysis
- **Multi-Source News**: NewsAPI, Alpaca, and more
- **Sentiment Analysis**: VADER + TextBlob hybrid analysis
- **Real-Time Monitoring**: Track news for portfolio stocks
- **Sentiment-to-Signal**: Convert sentiment into trading signals

### Personality Profiles
Choose from 7 pre-configured trading personalities:
- **Conservative Income Seeker**: Low risk, stable returns
- **Balanced Growth**: Moderate risk, steady growth
- **Aggressive Growth**: High risk, maximum returns
- **Day Trader Scalper**: High-frequency micro profits
- **Value Investor**: Long-term fundamental investing
- **Momentum Trader**: Ride trending stocks
- **AI Optimized**: Fully ML-driven, self-adapting

### Daily Operations
- **Morning Preview**: Pre-market analysis and predictions
- **Continuous Monitoring**: Real-time market tracking
- **Mid-Day Review**: Performance check
- **End-of-Day Analysis**: Comprehensive daily report
- **After-Hours Learning**: Learn from the day's performance
- **Weekly Reviews**: Performance analysis and model retraining

## 📦 Installation

### Prerequisites
```bash
Python 3.8+
pip
virtualenv (recommended)
```

### Setup
```bash
# Clone the repository
cd ai_trading_bot

# Create virtual environment
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Install TA-Lib (required for technical analysis)
# On Ubuntu/Debian:
sudo apt-get install ta-lib

# On macOS:
brew install ta-lib

# On Windows: Download from https://www.lfd.uci.edu/~gohlke/pythonlibs/#ta-lib
```

### Configuration

1. **Create Environment File**
```bash
cp .env.example .env
```

2. **Add API Keys** (edit `.env`):
```bash
# Trading API (Alpaca for paper/live trading)
ALPACA_API_KEY=your_alpaca_key
ALPACA_SECRET_KEY=your_alpaca_secret

# News API (optional but recommended)
NEWS_API_KEY=your_newsapi_key

# Alpha Vantage (optional, for fundamentals)
ALPHA_VANTAGE_KEY=your_alpha_vantage_key
```

3. **Customize Configuration** (edit `config/config.yaml`):
- Adjust risk parameters
- Set trading preferences
- Configure strategies
- Modify ML model settings

## 🚀 Usage

### Quick Start - Paper Trading
```bash
python main.py trade --mode paper --personality balanced_growth --capital 100000
```

### Backtesting
```bash
# Test on SPY
python main.py backtest --symbol SPY --start-date 2020-01-01 --end-date 2023-12-31

# Test with output
python main.py backtest --symbol AAPL --start-date 2022-01-01 --output results.csv
```

### Show Personality Profiles
```bash
python main.py profiles
```

### Show Configuration
```bash
python main.py config
```

## 🎨 Personality Profiles

### Conservative Income Seeker
- Risk: Low
- Focus: Stable, dividend-paying stocks
- Max Trades/Day: 5
- Position Size: 5% max

### Balanced Growth
- Risk: Moderate
- Focus: Steady growth with diversification
- Max Trades/Day: 20
- Position Size: 10% max

### Aggressive Growth
- Risk: High
- Focus: Maximum capital appreciation
- Max Trades/Day: 100
- Position Size: 15% max

### Day Trader Scalper
- Risk: Moderate
- Focus: High-frequency micro profits
- Max Trades/Day: 1000
- Position Size: 8% max
- Typical Hold Time: < 1 hour

### Value Investor
- Risk: Low
- Focus: Undervalued stocks, fundamentals
- Max Trades/Day: 3
- Position Size: 12% max
- Min Hold Time: 90 days

### Momentum Trader
- Risk: High
- Focus: Trending stocks, breakouts
- Max Trades/Day: 30
- Position Size: 12% max

### AI Optimized
- Risk: Moderate (adaptive)
- Focus: ML-driven decisions
- Max Trades/Day: 50
- Position Size: 10% max
- Self-adjusting parameters

## 📊 Example: Custom Trading Script

```python
from ai_trading_bot.core.trading_bot import TradingBot
from ai_trading_bot.core.personality_profiles import create_custom_profile

# Create custom personality
my_profile = create_custom_profile(
    name="My Custom Strategy",
    base_profile="balanced_growth",
    max_position_size=0.08,
    preferred_strategies=["momentum", "ml_hybrid"],
    max_daily_trades=50
)

# Initialize bot
bot = TradingBot(
    initial_capital=50000,
    personality=my_profile,
    mode="paper"
)

# Start trading
bot.start()
```

## 📈 Backtesting Example

```python
from ai_trading_bot.backtesting import BacktestEngine, StrategyEvaluator
from ai_trading_bot.backtesting.strategies import STRATEGY_REGISTRY
from ai_trading_bot.data import MarketDataCollector

# Get data
collector = MarketDataCollector()
df = collector.get_historical_data('AAPL', start_date='2020-01-01')

# Initialize backtest
engine = BacktestEngine(initial_capital=100000)

# Test momentum strategy
from ai_trading_bot.backtesting.strategies import momentum_strategy

results = engine.run_backtest(
    df,
    momentum_strategy,
    {'lookback': 20, 'threshold': 0.02, 'position_size': 0.1}
)

print(f"Total Return: {results['total_return']:.2f}%")
print(f"Sharpe Ratio: {results['sharpe_ratio']:.2f}")
print(f"Max Drawdown: {results['max_drawdown']:.2f}%")
print(f"Win Rate: {results['win_rate']:.2f}%")
```

## 🔧 Advanced Features

### Custom Strategy Development
Create your own strategies by following the strategy template:

```python
def my_custom_strategy(data, engine, params):
    """Your custom strategy logic."""
    signals = []

    # Your analysis here
    current_price = data['close'].iloc[-1]
    symbol = 'YOUR_SYMBOL'

    # Generate buy signal
    if your_buy_condition:
        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': calculate_quantity()
        })

    # Generate sell signal
    elif your_sell_condition:
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    return signals
```

### Accessing the Database
```python
from ai_trading_bot.utils.database import Database

db = Database()

# Get prediction performance
perf = db.get_prediction_performance(days=30)

# Get trade history
trades = db.get_trades_history(days=7)

# Get learning log
learning = db.get_learning_history(days=14)
```

### Monitoring Learning
```python
# Check what the AI has learned
learning_log = db.get_learning_history(days=30)

for entry in learning_log.itertuples():
    print(f"Learning Type: {entry.learning_type}")
    print(f"Description: {entry.description}")
    print(f"Previous: {entry.previous_behavior}")
    print(f"New: {entry.new_behavior}")
    print(f"Expected Improvement: {entry.expected_improvement}")
```

## 📁 Project Structure

```
ai_trading_bot/
├── config/                 # Configuration files
│   ├── config.yaml        # Main configuration
│   └── __init__.py
├── core/                  # Core trading bot
│   ├── trading_bot.py    # Main orchestrator
│   └── personality_profiles.py
├── ml_models/            # Machine learning
│   ├── prediction_model.py
│   ├── model_trainer.py
│   └── feature_engineering.py
├── backtesting/          # Backtesting engine
│   ├── backtest_engine.py
│   ├── strategies.py
│   └── strategy_evaluator.py
├── data/                 # Data collection
│   ├── market_data.py
│   ├── news_collector.py
│   └── sentiment_analyzer.py
├── portfolio/            # Portfolio management
│   ├── portfolio_manager.py
│   ├── position_tracker.py  # FIFO tracking
│   └── risk_manager.py
├── utils/                # Utilities
│   ├── database.py       # SQLite database
│   └── logger.py
├── database/             # Database files
├── logs/                 # Log files
├── models/               # Saved ML models
├── main.py              # Entry point
└── requirements.txt     # Dependencies
```

## 🛡️ Risk Management

The bot includes comprehensive risk management:
- **Position Sizing**: Kelly Criterion, fixed percentage
- **Stop Loss**: Fixed, trailing, ATR-based
- **Take Profit**: Multiple targets with scaling out
- **Daily Loss Limit**: Circuit breaker at 5% daily loss
- **Correlation Limits**: Avoid highly correlated positions
- **Sector Allocation**: Max 25% per sector
- **Diversification**: Minimum 10 positions

## 📊 Performance Tracking

The bot tracks and learns from:
- **Prediction Accuracy**: How accurate were ML predictions?
- **Trade Performance**: Win rate, profit factor, Sharpe ratio
- **Strategy Effectiveness**: Which strategies work best?
- **Risk Metrics**: Drawdown, volatility, VaR
- **Learning Events**: What did the bot learn and when?

## ⚠️ Important Notes

### This is for Educational Purposes
- This bot is designed for learning and research
- Always test thoroughly in paper trading mode
- Never risk more than you can afford to lose
- Past performance doesn't guarantee future results

### API Requirements
- **Alpaca**: Free paper trading account
- **NewsAPI**: Free tier available (100 requests/day)
- **Alpha Vantage**: Free tier available (5 requests/minute)

### Performance Considerations
- ML training can be CPU/GPU intensive
- Real-time data requires stable internet
- Database grows over time (cleanup recommended)

## 🤝 Contributing

Contributions are welcome! Areas for improvement:
- Additional trading strategies
- More ML models (transformers, reinforcement learning)
- Additional data sources
- Performance optimizations
- Dashboard/UI development

## 📄 License

MIT License - see LICENSE file

## 🙏 Acknowledgments

- Built with Python, scikit-learn, TensorFlow
- Data from yfinance, NewsAPI
- Technical analysis with TA-Lib

## 📞 Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Read the documentation in `/docs`
- Check the example scripts in `/examples`

---

**Disclaimer**: This software is for educational purposes only. Trading involves risk. The developers are not responsible for any financial losses.
