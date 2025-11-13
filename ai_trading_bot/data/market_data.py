"""Market data collection from various sources."""

import pandas as pd
import numpy as np
from typing import List, Optional, Dict, Any
from datetime import datetime, timedelta
import yfinance as yf

from ..utils.logger import get_logger
from ..config import config

logger = get_logger(__name__)


class MarketDataCollector:
    """Collects market data from multiple sources."""

    def __init__(self):
        self.data_source = config.get('data.sources.market_data.primary', 'yfinance')
        self.cache = {}

    def get_historical_data(
        self,
        symbol: str,
        start_date: str = None,
        end_date: str = None,
        interval: str = '1d'
    ) -> pd.DataFrame:
        """Get historical OHLCV data for a symbol."""

        try:
            logger.info(f"Fetching data for {symbol}")

            if start_date is None:
                start_date = (datetime.now() - timedelta(days=365)).strftime('%Y-%m-%d')
            if end_date is None:
                end_date = datetime.now().strftime('%Y-%m-%d')

            # Use yfinance
            ticker = yf.Ticker(symbol)
            df = ticker.history(start=start_date, end=end_date, interval=interval)

            if df.empty:
                logger.warning(f"No data available for {symbol}")
                return pd.DataFrame()

            # Standardize column names
            df.columns = [col.lower() for col in df.columns]

            # Add symbol column
            df['symbol'] = symbol

            logger.info(f"Retrieved {len(df)} bars for {symbol}")

            return df

        except Exception as e:
            logger.error(f"Error fetching data for {symbol}: {e}")
            return pd.DataFrame()

    def get_real_time_quote(self, symbol: str) -> Dict[str, Any]:
        """Get real-time quote for a symbol."""

        try:
            ticker = yf.Ticker(symbol)
            info = ticker.info

            quote = {
                'symbol': symbol,
                'price': info.get('currentPrice', info.get('regularMarketPrice', 0)),
                'bid': info.get('bid', 0),
                'ask': info.get('ask', 0),
                'volume': info.get('volume', 0),
                'timestamp': datetime.now()
            }

            return quote

        except Exception as e:
            logger.error(f"Error fetching quote for {symbol}: {e}")
            return {}

    def get_multiple_symbols(
        self,
        symbols: List[str],
        start_date: str = None,
        end_date: str = None
    ) -> Dict[str, pd.DataFrame]:
        """Get historical data for multiple symbols."""

        logger.info(f"Fetching data for {len(symbols)} symbols")

        data = {}
        for symbol in symbols:
            df = self.get_historical_data(symbol, start_date, end_date)
            if not df.empty:
                data[symbol] = df

        return data

    def get_fundamentals(self, symbol: str) -> Dict[str, Any]:
        """Get fundamental data for a symbol."""

        try:
            ticker = yf.Ticker(symbol)
            info = ticker.info

            fundamentals = {
                'symbol': symbol,
                'pe_ratio': info.get('trailingPE', None),
                'forward_pe': info.get('forwardPE', None),
                'pb_ratio': info.get('priceToBook', None),
                'ps_ratio': info.get('priceToSalesTrailing12Months', None),
                'peg_ratio': info.get('pegRatio', None),
                'debt_to_equity': info.get('debtToEquity', None),
                'roe': info.get('returnOnEquity', None),
                'roa': info.get('returnOnAssets', None),
                'profit_margin': info.get('profitMargins', None),
                'market_cap': info.get('marketCap', None),
                'enterprise_value': info.get('enterpriseValue', None),
                'revenue_growth': info.get('revenueGrowth', None),
                'earnings_growth': info.get('earningsGrowth', None),
                'dividend_yield': info.get('dividendYield', None),
                'sector': info.get('sector', None),
                'industry': info.get('industry', None)
            }

            return fundamentals

        except Exception as e:
            logger.error(f"Error fetching fundamentals for {symbol}: {e}")
            return {}

    def screen_stocks(
        self,
        min_price: float = 5.0,
        min_volume: int = 1000000,
        min_market_cap: float = 1e9
    ) -> List[str]:
        """Screen for stocks meeting criteria."""

        logger.info("Screening stocks...")

        # This is a simplified version
        # In production, you'd use a proper stock screener API

        # Common liquid stocks
        candidates = [
            'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'TSLA', 'META', 'NVDA', 'AMD',
            'SPY', 'QQQ', 'IWM', 'DIA',
            'JPM', 'BAC', 'WFC', 'GS',
            'XOM', 'CVX', 'COP',
            'JNJ', 'UNH', 'PFE',
            'WMT', 'HD', 'COST',
            'DIS', 'NFLX', 'CMCSA'
        ]

        filtered = []

        for symbol in candidates:
            try:
                quote = self.get_real_time_quote(symbol)
                fundamentals = self.get_fundamentals(symbol)

                if (quote.get('price', 0) >= min_price and
                    quote.get('volume', 0) >= min_volume and
                    fundamentals.get('market_cap', 0) >= min_market_cap):

                    filtered.append(symbol)

            except Exception as e:
                logger.warning(f"Error screening {symbol}: {e}")
                continue

        logger.info(f"Found {len(filtered)} stocks meeting criteria")

        return filtered

    def calculate_correlations(self, symbols: List[str], period: int = 60) -> pd.DataFrame:
        """Calculate correlation matrix for symbols."""

        logger.info(f"Calculating correlations for {len(symbols)} symbols")

        # Get data
        data = {}
        for symbol in symbols:
            df = self.get_historical_data(symbol)
            if not df.empty and len(df) >= period:
                data[symbol] = df['close'].tail(period)

        # Create correlation matrix
        prices_df = pd.DataFrame(data)
        correlations = prices_df.pct_change().corr()

        return correlations

    def get_market_regime(self, symbol: str = 'SPY') -> str:
        """Determine current market regime."""

        df = self.get_historical_data(symbol)

        if df.empty or len(df) < 50:
            return 'unknown'

        # Calculate indicators
        returns = df['close'].pct_change()
        volatility = returns.std()
        trend = (df['close'].iloc[-1] / df['close'].iloc[-20]) - 1

        # Determine regime
        if trend > 0.05 and volatility < returns.mean():
            return 'bull_trending'
        elif trend < -0.05 and volatility < returns.mean():
            return 'bear_trending'
        elif volatility > returns.std() * 1.5:
            return 'high_volatility'
        else:
            return 'ranging'
