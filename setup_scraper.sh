#!/bin/bash
# Setup script for Escape Room News Scraper

echo "================================"
echo "Escape Room News Scraper Setup"
echo "================================"
echo ""

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python 3 is not installed. Please install Python 3.7 or higher."
    exit 1
fi

echo "✓ Python 3 found: $(python3 --version)"
echo ""

# Install dependencies
echo "Installing Python dependencies..."
pip3 install -r requirements.txt

if [ $? -eq 0 ]; then
    echo "✓ Dependencies installed successfully"
else
    echo "ERROR: Failed to install dependencies"
    exit 1
fi
echo ""

# Check if .env exists
if [ ! -f .env ]; then
    echo "Creating .env file from template..."
    cp .env.example .env
    echo "✓ .env file created"
    echo ""
    echo "⚠️  IMPORTANT: Please edit .env file and add your email configuration!"
    echo ""
    echo "You need to configure:"
    echo "  - RECIPIENT_EMAIL (your email address)"
    echo "  - SENDER_EMAIL (Gmail or other email to send from)"
    echo "  - SENDER_PASSWORD (app password for sender email)"
    echo ""
    echo "For Gmail users:"
    echo "  1. Enable 2-Factor Authentication"
    echo "  2. Generate an App Password at: https://myaccount.google.com/security"
    echo "  3. Use the app password (not your regular password)"
    echo ""
    read -p "Press Enter to open .env file for editing..."
    ${EDITOR:-nano} .env
else
    echo "✓ .env file already exists"
fi
echo ""

# Make scraper executable
chmod +x escape_room_scraper.py
echo "✓ Made scraper executable"
echo ""

echo "================================"
echo "Setup Complete!"
echo "================================"
echo ""
echo "To run the scraper:"
echo "  python3 escape_room_scraper.py"
echo ""
echo "To set up automatic daily runs:"
echo "  crontab -e"
echo "  Add: 0 9 * * * cd $(pwd) && /usr/bin/python3 escape_room_scraper.py >> scraper.log 2>&1"
echo ""
echo "For more information, see README_SCRAPER.md"
echo ""
