# Quick Start Guide - Fort Lauderdale House Search

## 5-Minute Setup

### 1. Install Dependencies
```bash
pip install requests beautifulsoup4 lxml schedule python-dotenv
```

### 2. Configure Email (Optional but Recommended)
```bash
cp .env.example .env
# Edit .env with your email settings
```

For Gmail:
1. Go to https://myaccount.google.com/apppasswords
2. Generate an app password
3. Add to .env file

### 3. Run Your First Search
```bash
python3 fort_lauderdale_house_search.py
```

Answer the questions about your search criteria:
- Budget: e.g., 200000 to 500000
- Bedrooms: e.g., 3
- Bathrooms: e.g., 2
- Square feet: e.g., 1500
- Property types: single-family, condo, townhouse (or leave blank)
- Neighborhoods: Las Olas, Victoria Park, etc. (or leave blank)
- HOA max: e.g., 300
- Property age: e.g., 50
- Focus: i (investment) or p (personal)
- Email: your-email@example.com

### 4. Set Up Daily Search

**Option A - Keep Script Running:**
```bash
python3 run_daily_search.py
```

**Option B - Cron Job (Linux/Mac):**
```bash
./setup_daily_cron.sh
```

**Option C - Windows Task Scheduler:**
- See HOUSE_SEARCH_README.md for instructions

## What Happens Next?

- The script searches multiple sources for Fort Lauderdale properties
- Checks Real Broward County auctions for foreclosure opportunities
- Analyzes each property based on your criteria
- Scores properties from 0-100
- Emails you a beautiful HTML report with the best opportunities

## Example Output

You'll receive:
- Top properties matching your criteria
- Analysis scores (0-100) for each property
- Investment metrics (cap rate, cash flow estimates)
- Direct links to listings
- New listings highlighted

## Important Sources

The tool checks:
- Real Broward County Foreclosure Auctions (https://www.realbroward.org/)
- Multiple real estate listing sources
- Public property records
- Sample data for demonstration

## Tips for Best Results

1. **Be realistic with budget**: Fort Lauderdale median home price is ~$450k
2. **Consider flood zones**: Critical in South Florida
3. **Check auction properties carefully**: Sold as-is, may need work
4. **Visit properties in person**: Never buy based on data alone
5. **Consult professionals**: Real estate agent, inspector, attorney

## Troubleshooting

**No email received?**
- Check your .env configuration
- Look for HTML report files saved locally
- Check house_search.log for errors

**No properties found?**
- Your criteria may be too strict
- Try wider budget range
- Fewer neighborhood restrictions
- Lower minimum square footage

**Script errors?**
- Ensure all dependencies installed
- Check internet connection
- Review house_search.log

## Next Steps

1. Review your first report
2. Adjust search criteria if needed: Edit search_criteria.json
3. Set up daily automation
4. Start finding your dream property or investment!

---

For detailed documentation, see HOUSE_SEARCH_README.md
