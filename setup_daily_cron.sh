#!/bin/bash
# Setup script for daily cron job

echo "Fort Lauderdale House Search - Cron Setup"
echo "=========================================="
echo ""

# Get the current directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PYTHON_PATH=$(which python3)

echo "Script directory: $SCRIPT_DIR"
echo "Python path: $PYTHON_PATH"
echo ""

# Ask for time
echo "What time should the search run daily?"
read -p "Enter hour (0-23): " HOUR
read -p "Enter minute (0-59): " MINUTE

# Validate input
if ! [[ "$HOUR" =~ ^[0-9]+$ ]] || [ "$HOUR" -lt 0 ] || [ "$HOUR" -gt 23 ]; then
    echo "Invalid hour! Using 9 AM"
    HOUR=9
    MINUTE=0
fi

if ! [[ "$MINUTE" =~ ^[0-9]+$ ]] || [ "$MINUTE" -lt 0 ] || [ "$MINUTE" -gt 59 ]; then
    echo "Invalid minute! Using :00"
    MINUTE=0
fi

# Create cron entry
CRON_ENTRY="$MINUTE $HOUR * * * cd $SCRIPT_DIR && $PYTHON_PATH fort_lauderdale_house_search.py >> $SCRIPT_DIR/house_search_cron.log 2>&1"

echo ""
echo "This cron entry will be added:"
echo "$CRON_ENTRY"
echo ""

read -p "Add this to your crontab? (y/n): " CONFIRM

if [[ $CONFIRM =~ ^[Yy]$ ]]; then
    # Add to crontab
    (crontab -l 2>/dev/null; echo "$CRON_ENTRY") | crontab -
    echo ""
    echo "✓ Cron job added successfully!"
    echo "The search will run daily at $HOUR:$(printf "%02d" $MINUTE)"
    echo ""
    echo "To view your crontab: crontab -l"
    echo "To remove this job: crontab -e (then delete the line)"
    echo ""
else
    echo "Cron job not added"
    echo ""
    echo "To add manually, run: crontab -e"
    echo "Then add this line:"
    echo "$CRON_ENTRY"
fi

echo ""
echo "Setup complete!"
