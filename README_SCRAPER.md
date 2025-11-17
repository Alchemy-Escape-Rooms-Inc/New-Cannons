# Escape Room Industry News Scraper

An automated web scraper that monitors escape room industry websites for new articles and sends email notifications with interactive feedback options.

## Features

- **Multi-Source Monitoring**: Scrapes 10+ escape room industry news sources, blogs, and publications
- **Smart Deduplication**: Tracks previously seen articles to only notify you of new content
- **Email Notifications**: Sends beautifully formatted HTML emails with new articles
- **Interactive Feedback**: Click links in emails to provide feedback on articles
- **Easy Configuration**: Simple .env file configuration
- **Customizable Sources**: Easily add or remove news sources
- **Respectful Scraping**: Built-in delays between requests to be respectful to websites

## Monitored Sources

The scraper currently monitors these escape room industry sources:

1. **Escape Industry News** - Monthly newsletter with global industry news
2. **Room Escape Artist** - Industry reports, reviews, and event coverage
3. **The Escape Roomer** - News, reviews, and design tips
4. **Massive Impact** - Industry surveys and reports
5. **Peek Pro Blog** - Industry trends and business insights
6. **Globe Newswire** - Market research and analysis
7. **Allied Market Research** - Market forecasts and reports
8. **PanIQ Escape Room Blog** - Updates from major escape room chain
9. **60out Blog** - Industry insights and updates
10. And more...

## Installation

### Prerequisites

- Python 3.7 or higher
- pip (Python package manager)

### Step 1: Install Dependencies

```bash
pip install -r requirements.txt
```

This installs:
- `requests` - For making HTTP requests
- `beautifulsoup4` - For parsing HTML
- `lxml` - For fast HTML parsing

### Step 2: Configure Email Settings

1. Copy the example environment file:
   ```bash
   cp .env.example .env
   ```

2. Edit `.env` and fill in your email settings:
   ```bash
   nano .env
   # or use your preferred text editor
   ```

3. Configure these settings:
   - `RECIPIENT_EMAIL`: Your email address (where you'll receive updates)
   - `SENDER_EMAIL`: Email account to send from (e.g., Gmail account)
   - `SENDER_PASSWORD`: App password for the sender email
   - `SMTP_SERVER`: SMTP server (default: smtp.gmail.com for Gmail)
   - `SMTP_PORT`: SMTP port (default: 587)
   - `SEND_EMAIL`: Set to `true` to enable email notifications

## Email Setup Guide

### For Gmail Users

1. **Enable 2-Factor Authentication** on your Google account
2. **Create an App Password**:
   - Go to https://myaccount.google.com/security
   - Under "2-Step Verification", click "App passwords"
   - Select "Mail" and your device
   - Copy the generated 16-character password
   - Use this password in the `SENDER_PASSWORD` field

### For Other Email Providers

**Outlook/Hotmail**:
- SMTP Server: `smtp-mail.outlook.com`
- SMTP Port: `587`
- Enable "Let devices and apps use POP" in Outlook settings

**Yahoo**:
- SMTP Server: `smtp.mail.yahoo.com`
- SMTP Port: `587`
- Generate an app password in Yahoo account settings

## Usage

### Run the Scraper

```bash
python3 escape_room_scraper.py
```

The scraper will:
1. Check all configured news sources
2. Find new articles you haven't seen before
3. Send you an email with the new articles (if configured)
4. Save article IDs to avoid sending duplicates

### First Run

On the first run, the scraper will find many articles. This is normal! It will mark all current articles as "seen" and future runs will only show new articles published after the first run.

### Automation with Cron

To run the scraper automatically (e.g., daily):

1. Open crontab:
   ```bash
   crontab -e
   ```

2. Add a cron job (example: run daily at 9 AM):
   ```
   0 9 * * * cd /home/user/New-Cannons && /usr/bin/python3 escape_room_scraper.py >> scraper.log 2>&1
   ```

3. Or run twice daily (9 AM and 5 PM):
   ```
   0 9,17 * * * cd /home/user/New-Cannons && /usr/bin/python3 escape_room_scraper.py >> scraper.log 2>&1
   ```

## Email Feedback Features

Each email includes interactive feedback options:

### Article Feedback
- **👍 Yes, I liked it**: Click to indicate the article was useful
- **👎 Not relevant**: Click to indicate the article wasn't useful

### Source Management
- **Add New Sources**: Suggest websites to monitor
- **Remove Sources**: Request removal of sources you're not interested in
- **General Feedback**: Share thoughts and suggestions

All feedback is sent via email replies for easy tracking.

## Customizing News Sources

### Adding New Sources

1. Open `escape_room_sources.json`
2. Add a new source to the `sources` array:
   ```json
   {
     "name": "New Source Name",
     "url": "https://example.com/blog",
     "type": "blog",
     "description": "Description of the source"
   }
   ```

### Removing Sources

1. Open `escape_room_sources.json`
2. Remove the source object from the `sources` array
3. Save the file

## Files Overview

```
.
├── escape_room_scraper.py      # Main scraper script
├── escape_room_sources.json    # List of news sources to monitor
├── seen_articles.json          # Cache of previously seen articles (auto-generated)
├── requirements.txt            # Python dependencies
├── .env.example               # Example environment configuration
├── .env                       # Your email configuration (create this)
└── README_SCRAPER.md          # This file
```

## How It Works

1. **Load Sources**: Reads news sources from `escape_room_sources.json`
2. **Scrape Websites**: Uses BeautifulSoup to parse each website and find articles
3. **Detect New Articles**: Compares found articles against `seen_articles.json`
4. **Generate Email**: Creates an HTML email with new articles and feedback links
5. **Send Notification**: Sends email via SMTP
6. **Update Cache**: Saves newly seen articles to prevent duplicates

## Troubleshooting

### No Email Received

1. Check spam/junk folder
2. Verify email configuration in `.env`
3. For Gmail, ensure you're using an App Password, not your regular password
4. Check the script output for error messages

### "Authentication Failed" Error

- Double-check your email and app password
- Ensure 2-factor authentication is enabled (for Gmail)
- Verify SMTP server and port are correct

### No New Articles Found

- This is normal if no new content has been published
- The scraper only reports articles it hasn't seen before
- Try deleting `seen_articles.json` to reset (will re-send all articles)

### Scraping Errors

- Some websites may block automated requests
- The scraper uses a respectful 2-second delay between requests
- Check your internet connection
- Some sites may have changed their HTML structure

## Security Notes

- Never commit your `.env` file to version control (it's in .gitignore)
- Use app passwords instead of your main email password
- Keep your `SENDER_PASSWORD` secure
- The scraper doesn't collect or store personal data beyond article titles/URLs

## Feedback Processing

While the email includes feedback links that open your email client, you can also process feedback programmatically by:

1. Setting up a web endpoint to handle feedback URLs
2. Using email parsing to automatically process feedback replies
3. Building a simple feedback database

For now, feedback is handled via email replies for simplicity.

## Contributing

Want to improve the scraper? Ideas for enhancements:

- Add RSS feed support
- Implement sentiment analysis on articles
- Add web dashboard for feedback management
- Support for more news sources
- Better article summarization
- Mobile app notifications

## License

This project is open source and available for personal and commercial use.

## Support

If you encounter issues:

1. Check the troubleshooting section above
2. Review the script output for error messages
3. Ensure all dependencies are installed
4. Verify your email configuration

## Updates

To update the news sources list, simply edit `escape_room_sources.json` and the scraper will use the new sources on the next run.

---

**Happy Scraping! 🔐**

Stay informed about the escape room industry with automated news delivered right to your inbox!
