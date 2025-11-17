#!/usr/bin/env python3
"""
Daily scheduler for Fort Lauderdale house search
Run this script to have the search execute automatically every day
"""

import schedule
import time
from fort_lauderdale_house_search import RealEstateSearcher, load_criteria
import logging
from datetime import datetime

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('daily_search.log'),
        logging.StreamHandler()
    ]
)


def run_daily_search():
    """Execute the daily search"""
    logging.info("="*60)
    logging.info(f"Starting daily search at {datetime.now()}")
    logging.info("="*60)

    try:
        # Load saved criteria
        criteria = load_criteria()
        if not criteria:
            logging.error("No saved criteria found! Please run fort_lauderdale_house_search.py first.")
            return

        # Run search
        searcher = RealEstateSearcher(criteria)
        properties = searcher.search_all_sources()

        logging.info(f"Found {len(properties)} properties")

        # Send email report
        if properties:
            searcher.send_email_report()
            logging.info("Email report sent successfully")
        else:
            logging.info("No properties found matching criteria")

    except Exception as e:
        logging.error(f"Error during daily search: {e}", exc_info=True)

    logging.info("="*60)
    logging.info("Daily search complete")
    logging.info("="*60 + "\n")


def main():
    """Main scheduler loop"""
    print("Fort Lauderdale House Search - Daily Scheduler")
    print("="*60)

    # Ask user what time to run
    print("\nWhat time should the search run daily?")
    search_time = input("Enter time (HH:MM in 24-hour format, e.g., 09:00): ").strip()

    # Validate time format
    try:
        time.strptime(search_time, "%H:%M")
    except ValueError:
        print("Invalid time format! Using default time: 09:00")
        search_time = "09:00"

    print(f"\nScheduling daily search at {search_time}")

    # Schedule the job
    schedule.every().day.at(search_time).do(run_daily_search)

    # Option to run immediately
    run_now = input("\nRun search now? (y/n): ").strip().lower()
    if run_now.startswith('y'):
        run_daily_search()

    print(f"\nScheduler is running. Search will execute daily at {search_time}")
    print("Press Ctrl+C to stop\n")

    # Keep the script running
    try:
        while True:
            schedule.run_pending()
            time.sleep(60)  # Check every minute
    except KeyboardInterrupt:
        print("\n\nScheduler stopped by user")


if __name__ == "__main__":
    main()
