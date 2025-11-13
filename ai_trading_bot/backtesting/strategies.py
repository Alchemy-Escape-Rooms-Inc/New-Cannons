"""Pre-built trading strategies for backtesting."""

import numpy as np
import pandas as pd
from typing import Dict, List, Any
import talib as ta


def momentum_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """Momentum-based trading strategy."""

    signals = []

    if len(data) < params.get('lookback', 20):
        return signals

    # Get parameters
    lookback = params.get('lookback', 20)
    threshold = params.get('threshold', 0.02)

    # Calculate momentum
    current_price = data['close'].iloc[-1]
    past_price = data['close'].iloc[-lookback]
    momentum = (current_price - past_price) / past_price

    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Generate signals
    if momentum > threshold and symbol not in engine.open_positions:
        # Buy signal
        position_size = engine.capital * params.get('position_size', 0.1)
        quantity = position_size / current_price

        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': quantity
        })

    elif momentum < -threshold and symbol in engine.open_positions:
        # Sell signal
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    return signals


def mean_reversion_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """Mean reversion strategy using Bollinger Bands."""

    signals = []

    if len(data) < params.get('period', 20):
        return signals

    period = params.get('period', 20)
    std_dev = params.get('std_dev', 2)

    # Calculate Bollinger Bands
    sma = data['close'].rolling(window=period).mean()
    std = data['close'].rolling(window=period).std()
    upper_band = sma + (std * std_dev)
    lower_band = sma - (std * std_dev)

    current_price = data['close'].iloc[-1]
    current_upper = upper_band.iloc[-1]
    current_lower = lower_band.iloc[-1]
    current_sma = sma.iloc[-1]

    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Buy when price touches lower band
    if current_price <= current_lower and symbol not in engine.open_positions:
        position_size = engine.capital * params.get('position_size', 0.1)
        quantity = position_size / current_price

        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': quantity
        })

    # Sell when price reaches SMA or upper band
    elif symbol in engine.open_positions:
        if current_price >= current_sma:
            signals.append({
                'action': 'sell',
                'symbol': symbol,
                'price': current_price
            })

    return signals


def trend_following_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """Trend following using moving average crossovers."""

    signals = []

    short_period = params.get('short_period', 20)
    long_period = params.get('long_period', 50)

    if len(data) < long_period:
        return signals

    # Calculate moving averages
    short_ma = data['close'].rolling(window=short_period).mean()
    long_ma = data['close'].rolling(window=long_period).mean()

    # Get current and previous values
    current_short = short_ma.iloc[-1]
    current_long = long_ma.iloc[-1]
    prev_short = short_ma.iloc[-2]
    prev_long = long_ma.iloc[-2]

    current_price = data['close'].iloc[-1]
    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Bullish crossover
    if prev_short <= prev_long and current_short > current_long:
        if symbol not in engine.open_positions:
            position_size = engine.capital * params.get('position_size', 0.1)
            quantity = position_size / current_price

            signals.append({
                'action': 'buy',
                'symbol': symbol,
                'price': current_price,
                'quantity': quantity
            })

    # Bearish crossover
    elif prev_short >= prev_long and current_short < current_long:
        if symbol in engine.open_positions:
            signals.append({
                'action': 'sell',
                'symbol': symbol,
                'price': current_price
            })

    return signals


def breakout_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """Breakout strategy based on price channels."""

    signals = []

    lookback = params.get('lookback', 20)

    if len(data) < lookback:
        return signals

    # Calculate high/low channels
    high_channel = data['high'].rolling(window=lookback).max()
    low_channel = data['low'].rolling(window=lookback).min()

    current_price = data['close'].iloc[-1]
    current_high_channel = high_channel.iloc[-2]  # Previous period to avoid looking ahead
    current_low_channel = low_channel.iloc[-2]

    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Breakout above resistance
    if current_price > current_high_channel and symbol not in engine.open_positions:
        position_size = engine.capital * params.get('position_size', 0.1)
        quantity = position_size / current_price

        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': quantity
        })

    # Break below support
    elif current_price < current_low_channel and symbol in engine.open_positions:
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    return signals


def rsi_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """RSI-based strategy for overbought/oversold conditions."""

    signals = []

    period = params.get('period', 14)
    oversold = params.get('oversold', 30)
    overbought = params.get('overbought', 70)

    if len(data) < period + 1:
        return signals

    # Calculate RSI
    rsi = ta.RSI(data['close'].values, timeperiod=period)

    current_rsi = rsi[-1]
    current_price = data['close'].iloc[-1]

    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Buy when oversold
    if current_rsi < oversold and symbol not in engine.open_positions:
        position_size = engine.capital * params.get('position_size', 0.1)
        quantity = position_size / current_price

        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': quantity
        })

    # Sell when overbought
    elif current_rsi > overbought and symbol in engine.open_positions:
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    return signals


def macd_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """MACD crossover strategy."""

    signals = []

    fast = params.get('fast', 12)
    slow = params.get('slow', 26)
    signal = params.get('signal', 9)

    if len(data) < slow + signal:
        return signals

    # Calculate MACD
    macd, macd_signal, macd_hist = ta.MACD(
        data['close'].values,
        fastperiod=fast,
        slowperiod=slow,
        signalperiod=signal
    )

    current_macd = macd[-1]
    current_signal = macd_signal[-1]
    prev_macd = macd[-2]
    prev_signal = macd_signal[-2]

    current_price = data['close'].iloc[-1]
    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Bullish crossover
    if prev_macd <= prev_signal and current_macd > current_signal:
        if symbol not in engine.open_positions:
            position_size = engine.capital * params.get('position_size', 0.1)
            quantity = position_size / current_price

            signals.append({
                'action': 'buy',
                'symbol': symbol,
                'price': current_price,
                'quantity': quantity
            })

    # Bearish crossover
    elif prev_macd >= prev_signal and current_macd < current_signal:
        if symbol in engine.open_positions:
            signals.append({
                'action': 'sell',
                'symbol': symbol,
                'price': current_price
            })

    return signals


def pairs_trading_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """Pairs trading / statistical arbitrage strategy."""

    signals = []

    # This is a simplified version - full implementation would need two correlated assets

    lookback = params.get('lookback', 20)
    entry_z = params.get('entry_z', 2.0)
    exit_z = params.get('exit_z', 0.5)

    if len(data) < lookback:
        return signals

    # Calculate z-score
    sma = data['close'].rolling(window=lookback).mean()
    std = data['close'].rolling(window=lookback).std()
    z_score = (data['close'] - sma) / std

    current_z = z_score.iloc[-1]
    current_price = data['close'].iloc[-1]

    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Buy when significantly below mean
    if current_z < -entry_z and symbol not in engine.open_positions:
        position_size = engine.capital * params.get('position_size', 0.1)
        quantity = position_size / current_price

        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': quantity
        })

    # Sell when reverting to mean
    elif abs(current_z) < exit_z and symbol in engine.open_positions:
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    # Also sell if goes too far in opposite direction
    elif current_z > entry_z and symbol in engine.open_positions:
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    return signals


def ml_hybrid_strategy(data: pd.DataFrame, engine, params: Dict[str, Any]) -> List[Dict]:
    """Hybrid strategy combining ML predictions with technical indicators."""

    signals = []

    # This would integrate with the ML models
    # Simplified version using technical indicators

    if len(data) < 50:
        return signals

    # Multiple confirmation signals
    confirmations = 0
    target_confirmations = params.get('min_confirmations', 3)

    current_price = data['close'].iloc[-1]

    # RSI confirmation
    rsi = ta.RSI(data['close'].values, timeperiod=14)
    if rsi[-1] < 30:
        confirmations += 1
    elif rsi[-1] > 70:
        confirmations -= 1

    # MACD confirmation
    macd, signal, hist = ta.MACD(data['close'].values)
    if macd[-1] > signal[-1] and macd[-2] <= signal[-2]:
        confirmations += 1
    elif macd[-1] < signal[-1] and macd[-2] >= signal[-2]:
        confirmations -= 1

    # Moving average confirmation
    sma_20 = data['close'].rolling(window=20).mean().iloc[-1]
    sma_50 = data['close'].rolling(window=50).mean().iloc[-1]
    if sma_20 > sma_50:
        confirmations += 1
    else:
        confirmations -= 1

    # Volume confirmation
    vol_avg = data['volume'].rolling(window=20).mean().iloc[-1]
    if data['volume'].iloc[-1] > vol_avg * 1.5:
        confirmations += 1

    symbol = data.get('symbol', ['DEFAULT']).iloc[-1] if 'symbol' in data.columns else 'DEFAULT'

    # Buy signal
    if confirmations >= target_confirmations and symbol not in engine.open_positions:
        position_size = engine.capital * params.get('position_size', 0.1)
        quantity = position_size / current_price

        signals.append({
            'action': 'buy',
            'symbol': symbol,
            'price': current_price,
            'quantity': quantity
        })

    # Sell signal
    elif confirmations <= -target_confirmations and symbol in engine.open_positions:
        signals.append({
            'action': 'sell',
            'symbol': symbol,
            'price': current_price
        })

    return signals


# Strategy registry
STRATEGY_REGISTRY = {
    'momentum': momentum_strategy,
    'mean_reversion': mean_reversion_strategy,
    'trend_following': trend_following_strategy,
    'breakout': breakout_strategy,
    'rsi': rsi_strategy,
    'macd': macd_strategy,
    'pairs_trading': pairs_trading_strategy,
    'ml_hybrid': ml_hybrid_strategy
}


# Default parameters for each strategy
DEFAULT_PARAMS = {
    'momentum': {
        'lookback': 20,
        'threshold': 0.02,
        'position_size': 0.1
    },
    'mean_reversion': {
        'period': 20,
        'std_dev': 2,
        'position_size': 0.1
    },
    'trend_following': {
        'short_period': 20,
        'long_period': 50,
        'position_size': 0.1
    },
    'breakout': {
        'lookback': 20,
        'position_size': 0.1
    },
    'rsi': {
        'period': 14,
        'oversold': 30,
        'overbought': 70,
        'position_size': 0.1
    },
    'macd': {
        'fast': 12,
        'slow': 26,
        'signal': 9,
        'position_size': 0.1
    },
    'pairs_trading': {
        'lookback': 20,
        'entry_z': 2.0,
        'exit_z': 0.5,
        'position_size': 0.1
    },
    'ml_hybrid': {
        'min_confirmations': 3,
        'position_size': 0.1
    }
}
