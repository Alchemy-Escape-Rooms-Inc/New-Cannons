# Fort Lauderdale Real Estate Search & Analysis Tool

An intelligent property search system that finds houses for sale in Fort Lauderdale, analyzes them for investment potential and personal residence suitability, and emails you daily reports.

## Features

- **Multi-Source Search**: Searches multiple real estate listing sources
- **Real Broward County Auctions**: Special focus on foreclosure auctions for below-market opportunities
- **Smart Analysis**: Evaluates properties based on:
  - Price per square foot
  - Cap rate and cash flow potential (for investments)
  - Neighborhood preferences
  - Property age and condition
  - HOA fees
  - Investment metrics (1% rule, cap rate)
- **Daily Email Reports**: Automatically searches and sends findings to your email
- **Customizable Criteria**: Set your budget, preferences, and requirements
- **Property Tracking**: Remembers properties you've seen to identify new listings

## Setup Instructions

### 1. Install Python Dependencies

```bash
pip install -r requirements.txt
```

Or install individually:
```bash
pip install requests beautifulsoup4 lxml schedule python-dotenv
```

### 2. Configure Email Settings

For the tool to send you email reports, you need to configure email credentials:

1. Copy the example environment file:
   ```bash
   cp .env.example .env
   ```

2. Edit `.env` and add your email credentials:
   ```
   SMTP_SERVER=smtp.gmail.com
   SMTP_PORT=587
   SENDER_EMAIL=your-email@gmail.com
   SENDER_PASSWORD=your-app-password
   ```

**For Gmail users:**
- You need to use an "App Password" instead of your regular password
- Go to: https://myaccount.google.com/apppasswords
- Generate a new app password
- Use that password in the `.env` file

**Note:** If you don't configure email, reports will be saved as HTML files locally instead.

### 3. Run Initial Setup

Run the search tool once to set your criteria:

```bash
python3 fort_lauderdale_house_search.py
```

You'll be asked to provide:
- Budget range (min/max)
- Minimum bedrooms and bathrooms
- Minimum square footage
- Property types (single-family, condo, townhouse, etc.)
- Preferred neighborhoods
- Maximum HOA fees
- Maximum property age
- Investment focus vs. personal residence
- Your email address

Your criteria will be saved to `search_criteria.json` for future runs.

## Usage

### One-Time Search

Run a single search with your saved criteria:

```bash
python3 fort_lauderdale_house_search.py
```

### Daily Automated Search

#### Option 1: Using the Scheduler Script (Recommended for testing)

Run this script and keep it running:

```bash
python3 run_daily_search.py
```

It will ask you what time to run the search daily, then keep running in the background.

**To run persistently:** Use `screen` or `tmux`:
```bash
# Start a screen session
screen -S house_search

# Run the scheduler
python3 run_daily_search.py

# Detach with: Ctrl+A, then D
# Reattach with: screen -r house_search
```

#### Option 2: Using Cron (Linux/Mac)

Add to your crontab for automatic daily runs:

```bash
# Edit crontab
crontab -e

# Add this line to run every day at 9 AM
0 9 * * * cd /path/to/New-Cannons && /usr/bin/python3 fort_lauderdale_house_search.py >> house_search_cron.log 2>&1
```

Replace `/path/to/New-Cannons` with the actual path to this directory.

#### Option 3: Using Windows Task Scheduler

1. Open Task Scheduler
2. Create Basic Task
3. Set trigger: Daily at your preferred time
4. Action: Start a program
   - Program: `python.exe`
   - Arguments: `fort_lauderdale_house_search.py`
   - Start in: Path to this directory

## Understanding the Analysis

The tool scores each property from 0-100 based on multiple factors:

### Investment Property Analysis
- **Cap Rate**: Estimated annual return on investment
  - Excellent: >8%
  - Good: 6-8%
  - Fair: 4-6%
- **1% Rule**: Monthly rent should be ≥1% of purchase price
- **Price per sqft**: Compared to Fort Lauderdale market ($300-400/sqft average)

### Personal Residence Analysis
- **Location**: Preference for specified neighborhoods
- **Property Age**: Newer properties score higher
- **Size**: Extra bedrooms beyond minimum requirements
- **HOA Fees**: Lower fees score higher

### Common Factors
- **Auction Properties**: Potential for below-market prices (+10 points)
- **Price/sqft**: Below-market pricing gets bonus points
- **HOA Fees**:
  - Low (<$200/mo): +10 points
  - High (>$500/mo): -5 points

### Recommendations
- **🌟 EXCELLENT** (50+ points): Strong opportunity, investigate immediately
- **✓ GOOD** (35-49 points): Worth serious consideration
- **Consider** (20-34 points): Review carefully, may work for specific needs
- **Review carefully** (<20 points): Meets basic criteria but may have drawbacks

## Files Created

- `search_criteria.json`: Your saved search preferences
- `properties.db`: SQLite database tracking properties you've seen
- `house_search.log`: Detailed log of search operations
- `house_search_report_*.html`: HTML reports (if email fails)
- `daily_search.log`: Log file for scheduled searches

## Customization

### Adding More Data Sources

Edit `fort_lauderdale_house_search.py` and add methods to the `RealEstateSearcher` class:

```python
def search_custom_source(self) -> List[Property]:
    """Search your custom data source"""
    properties = []
    # Add your scraping/API logic here
    return properties
```

Then add it to `search_all_sources()`:
```python
all_properties.extend(self.search_custom_source())
```

### Modifying Analysis Criteria

Edit the `analyze_property()` method to adjust scoring logic.

### Changing Email Format

Edit the `_generate_html_report()` method to customize the email layout and content.

## Important Notes

### Web Scraping Considerations

- Many real estate sites have terms of service that restrict automated access
- Consider using official APIs when available:
  - Zillow has a limited API (requires approval)
  - Realtor.com has an API through RapidAPI
  - Redfin has some public data endpoints

- Always respect `robots.txt` and rate limits
- Add delays between requests (the code includes 2-second delays)
- For production use, consider:
  - Using residential proxies
  - Rotating user agents
  - Implementing proper retry logic

### Data Accuracy

- Property data scraped from websites may not always be current or accurate
- Always verify details directly with the listing agent
- Rental income estimates are rough calculations
- Cap rate calculations are simplified (doesn't include all expenses)
- Use this tool as a screening mechanism, not as the sole basis for investment decisions

### Real Broward County Auctions

- Auction properties typically require:
  - Cash payment (or pre-approved financing)
  - Properties are sold "as-is"
  - May have liens or title issues
  - Often need renovation
  - Professional due diligence is essential

Visit https://www.realbroward.org/ for official auction information.

## Troubleshooting

### Email not sending
- Check your `.env` file has correct credentials
- For Gmail, ensure you're using an App Password, not your regular password
- Check the logs in `house_search.log`
- If email fails, reports are saved as HTML files locally

### No properties found
- Check that your criteria aren't too restrictive
- Verify your budget range is realistic for Fort Lauderdale market
- Look at `house_search.log` for errors
- Try broadening search criteria (fewer neighborhoods, wider price range)

### Script crashes
- Check `house_search.log` for error details
- Ensure all dependencies are installed: `pip install -r requirements.txt`
- Verify internet connection
- Some websites may block automated requests - check logs

### Database errors
- Delete `properties.db` to reset the tracking database
- It will be recreated automatically on next run

## Market Research Resources

The tool includes automated research, but also check these resources:

- **Broward County Property Appraiser**: https://web.bcpa.net/
- **Real Broward County Auctions**: https://www.realbroward.org/
- **Zillow**: https://www.zillow.com/fort-lauderdale-fl/
- **Realtor.com**: https://www.realtor.com/
- **Redfin**: https://www.redfin.com/
- **City of Fort Lauderdale**: https://www.fortlauderdale.gov/

## Investment Considerations

When evaluating properties, consider:

### Location Factors
- Proximity to beaches (premium in Fort Lauderdale)
- School districts (affects resale value)
- Crime rates
- Future development plans
- Flood zones (critical in South Florida)

### Financial Factors
- Property taxes (Florida has no state income tax but property taxes vary)
- Insurance costs (hurricane insurance is significant)
- HOA fees and restrictions
- Maintenance costs (older properties, pool maintenance, etc.)
- Vacancy rates (for rentals)

### Fort Lauderdale Specific
- Hurricane preparedness (impact windows, elevation)
- Proximity to water (beach, Intracoastal, canals)
- Tourist vs. residential areas
- Cruise port proximity (for short-term rentals)

## Legal Disclaimer

This tool is for informational purposes only. It does not constitute:
- Professional real estate advice
- Investment advice
- Legal advice
- Financial planning advice

Always consult with licensed professionals (real estate agents, attorneys, CPAs, financial advisors) before making real estate investment decisions.

## Support

For issues or questions:
1. Check the logs: `house_search.log` and `daily_search.log`
2. Verify your `.env` configuration
3. Ensure all dependencies are installed
4. Check your internet connection

## License

This tool is provided as-is for personal use.

---

Happy house hunting in Fort Lauderdale! 🏖️🏠
