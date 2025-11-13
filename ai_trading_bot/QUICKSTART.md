# Quick Start Guide

Get started with the AI Trading Bot in 5 minutes!

## Step 1: Install Dependencies

```bash
cd ai_trading_bot
pip install -r requirements.txt
```

## Step 2: Configure API Keys

1. Copy the example environment file:
```bash
cp .env.example .env
```

2. Edit `.env` and add your API keys:
```bash
ALPACA_API_KEY=your_key
ALPACA_SECRET_KEY=your_secret
NEWS_API_KEY=your_key  # Optional but recommended
```

**Get Free API Keys:**
- Alpaca (Paper Trading): https://alpaca.markets/
- NewsAPI: https://newsapi.org/

## Step 3: Run Your First Backtest

Test the bot's strategies on historical data:

```bash
python main.py backtest --symbol SPY --start-date 2022-01-01 --end-date 2023-12-31
```

You'll see results like:
```
BACKTEST RESULTS
================================================================================
strategy_name      total_return  sharpe_ratio  win_rate  max_drawdown
momentum                 15.2%          1.45     58.3%         -8.5%
mean_reversion           12.8%          1.32     55.1%         -7.2%
trend_following          18.5%          1.67     62.4%         -6.8%
...
```

## Step 4: Start Paper Trading

Start the bot in paper trading mode (no real money):

```bash
python main.py trade --mode paper --personality balanced_growth --capital 100000
```

The bot will:
- Analyze the market every morning
- Make predictions and track accuracy
- Execute trades based on its ML models
- Learn from results and adapt
- Provide daily performance reports

## Step 5: Check Different Personalities

See all available trading personalities:

```bash
python main.py profiles
```

Try different personalities:
```bash
# Conservative (low risk)
python main.py trade --mode paper --personality conservative_income

# Aggressive (high risk, high reward)
python main.py trade --mode paper --personality aggressive_growth

# Day trader (high frequency)
python main.py trade --mode paper --personality day_trader_scalper
```

## Understanding the Output

### Morning Report
```
PRE-MARKET ANALYSIS
================================================================================
PORTFOLIO SUMMARY
  Total Value: $100,523.45
  Cash: $45,230.12
  Total Return: 0.52%
  Open Positions: 8

MARKET SENTIMENT: POSITIVE
  Score: 0.342
  Articles Analyzed: 24

TOP PREDICTIONS FOR TODAY
  1. AAPL: UP (Confidence: 0.78, Expected Return: 1.52%)
  2. MSFT: UP (Confidence: 0.71, Expected Return: 1.21%)
  ...

TODAY'S FOCUS
  Strategy: momentum
  Max Trades: 20
  Risk Level: moderate
```

### End of Day Report
```
END OF DAY ANALYSIS
================================================================================
DAILY PERFORMANCE - 2024-01-15
  Portfolio Value: $101,245.67
  Daily P&L: $722.22
  Total Trades: 12
  Winning Trades: 8
  Losing Trades: 4
  Win Rate: 66.7%
  Predictions Made: 15
```

## Next Steps

### 1. Customize Configuration
Edit `config/config.yaml` to adjust:
- Risk parameters
- Trading strategies
- ML model settings
- Position sizing

### 2. Monitor Learning
Check what the bot is learning:
```python
from ai_trading_bot.utils.database import Database

db = Database()
learning = db.get_learning_history(days=7)
print(learning)
```

### 3. Develop Custom Strategies
Create your own trading strategies in `backtesting/strategies.py`

### 4. Optimize Parameters
Use the built-in optimizer:
```python
from ai_trading_bot.backtesting import BacktestEngine
from ai_trading_bot.data import MarketDataCollector

collector = MarketDataCollector()
df = collector.get_historical_data('AAPL')

engine = BacktestEngine()
best_params, performance = engine.optimize_parameters(
    df,
    momentum_strategy,
    {
        'lookback': [10, 20, 30],
        'threshold': [0.01, 0.02, 0.03],
        'position_size': [0.05, 0.10, 0.15]
    }
)
```

## Common Issues

### Issue: Import Errors
**Solution**: Make sure you're in the correct directory and have activated the virtual environment:
```bash
cd ai_trading_bot
source venv/bin/activate  # or venv\Scripts\activate on Windows
```

### Issue: API Key Errors
**Solution**: Check that:
1. `.env` file exists (copy from `.env.example`)
2. API keys are correctly entered
3. No extra spaces around keys

### Issue: TA-Lib Installation
**Solution**: TA-Lib requires compilation:
- Ubuntu/Debian: `sudo apt-get install ta-lib`
- macOS: `brew install ta-lib`
- Windows: Download wheel from https://www.lfd.uci.edu/~gohlke/pythonlibs/#ta-lib

### Issue: No Data Retrieved
**Solution**: Check:
1. Internet connection
2. Symbol name is correct
3. Date range is valid

## Tips for Best Results

1. **Start Small**: Begin with paper trading and small capital
2. **Backtest First**: Always backtest strategies before live trading
3. **Monitor Daily**: Check morning and evening reports
4. **Review Learning**: See what the bot learns weekly
5. **Adjust Gradually**: Make small configuration changes
6. **Diversify**: Use multiple strategies and stocks
7. **Set Limits**: Always use stop-losses and risk limits

## Getting Help

- Read the full README.md
- Check example scripts in `/examples`
- Review configuration in `config/config.yaml`
- Examine the database: `sqlite3 database/trading_bot.db`

## Live Trading Warning

⚠️ **Before going live**:
1. Test thoroughly in paper mode (minimum 30 days)
2. Verify all API connections
3. Start with very small capital
4. Monitor constantly for first week
5. Never risk more than you can afford to lose

---

**You're now ready to start!** Run the paper trading command and watch your AI bot learn and trade.
