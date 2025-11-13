"""Logging configuration for AI Trading Bot."""

import sys
from pathlib import Path
from loguru import logger
from typing import Optional


def setup_logger(
    log_file: str = "logs/trading_bot.log",
    level: str = "INFO",
    max_size: str = "100 MB",
    backup_count: int = 10
) -> None:
    """Configure the logger with file and console output."""

    # Create logs directory if it doesn't exist
    log_path = Path(log_file)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    # Remove default handler
    logger.remove()

    # Add console handler with colors
    logger.add(
        sys.stdout,
        level=level,
        format="<green>{time:YYYY-MM-DD HH:mm:ss}</green> | <level>{level: <8}</level> | <cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> | <level>{message}</level>",
        colorize=True
    )

    # Add file handler with rotation
    logger.add(
        log_file,
        level=level,
        format="{time:YYYY-MM-DD HH:mm:ss} | {level: <8} | {name}:{function}:{line} | {message}",
        rotation=max_size,
        retention=backup_count,
        compression="zip"
    )

    logger.info("Logger initialized successfully")


def get_logger(name: Optional[str] = None):
    """Get a logger instance."""
    if name:
        return logger.bind(name=name)
    return logger


# Initialize logger on import
setup_logger()
