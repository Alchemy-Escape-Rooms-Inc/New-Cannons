#!/usr/bin/env python3
"""
Fort Lauderdale Real Estate Search and Analysis Tool
Searches multiple sources for properties and analyzes them for investment potential
"""

import requests
from bs4 import BeautifulSoup
import json
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from datetime import datetime
import time
import os
from typing import List, Dict, Any
import re
import sqlite3
from dataclasses import dataclass, asdict
import logging

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('house_search.log'),
        logging.StreamHandler()
    ]
)

@dataclass
class SearchCriteria:
    """User's search criteria"""
    max_budget: float
    min_budget: float
    min_bedrooms: int
    min_bathrooms: float
    preferred_neighborhoods: List[str]
    property_types: List[str]  # single-family, condo, townhouse
    investment_focus: bool  # True if primarily investment, False if personal residence
    max_age_years: int  # Maximum property age
    min_sqft: int
    max_hoa: float  # Maximum HOA fees
    email: str

@dataclass
class Property:
    """Property information"""
    address: str
    price: float
    bedrooms: int
    bathrooms: float
    sqft: int
    year_built: int
    property_type: str
    source: str
    url: str
    neighborhood: str
    hoa_fees: float
    description: str
    listing_date: str
    analysis_score: float = 0.0
    recommendation: str = ""

class RealEstateSearcher:
    """Main class for searching and analyzing real estate"""

    def __init__(self, criteria: SearchCriteria):
        self.criteria = criteria
        self.properties = []
        self.db_path = "properties.db"
        self.init_database()

    def init_database(self):
        """Initialize SQLite database to track properties we've seen"""
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute('''CREATE TABLE IF NOT EXISTS seen_properties
                     (address TEXT PRIMARY KEY,
                      price REAL,
                      first_seen TEXT,
                      last_seen TEXT,
                      times_seen INTEGER)''')
        conn.commit()
        conn.close()

    def mark_property_seen(self, address: str, price: float):
        """Mark a property as seen in the database"""
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        now = datetime.now().isoformat()

        c.execute("SELECT times_seen FROM seen_properties WHERE address = ?", (address,))
        result = c.fetchone()

        if result:
            c.execute("""UPDATE seen_properties
                        SET last_seen = ?, times_seen = times_seen + 1, price = ?
                        WHERE address = ?""", (now, price, address))
        else:
            c.execute("""INSERT INTO seen_properties
                        (address, price, first_seen, last_seen, times_seen)
                        VALUES (?, ?, ?, ?, 1)""", (address, price, now, now))

        conn.commit()
        conn.close()

    def is_new_listing(self, address: str) -> bool:
        """Check if this is a new listing we haven't seen before"""
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute("SELECT address FROM seen_properties WHERE address = ?", (address,))
        result = c.fetchone()
        conn.close()
        return result is None

    def search_real_broward_auctions(self) -> List[Property]:
        """Search Real Broward County auction listings"""
        logging.info("Searching Real Broward County auctions...")
        properties = []

        try:
            # Real Broward County auction site
            url = "https://www.realbroward.org/foreclosure-auction-list"
            headers = {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
            }

            response = requests.get(url, headers=headers, timeout=30)

            if response.status_code == 200:
                soup = BeautifulSoup(response.content, 'html.parser')

                # Look for auction listings
                # Note: The actual HTML structure will vary - this is a template
                listings = soup.find_all(['div', 'tr'], class_=re.compile(r'(listing|property|auction)', re.I))

                for listing in listings[:50]:  # Limit to 50 properties
                    try:
                        property_data = self._parse_auction_listing(listing, url)
                        if property_data and self._matches_criteria(property_data):
                            properties.append(property_data)
                    except Exception as e:
                        logging.warning(f"Error parsing auction listing: {e}")

            logging.info(f"Found {len(properties)} matching auction properties")

        except Exception as e:
            logging.error(f"Error searching Real Broward auctions: {e}")

        return properties

    def _parse_auction_listing(self, listing, base_url: str) -> Property:
        """Parse individual auction listing"""
        # This is a template - actual parsing depends on site structure
        try:
            address = listing.find(text=re.compile(r'\d+.*Fort Lauderdale|FL', re.I))
            if not address:
                return None

            # Extract price (auction starting bid)
            price_text = listing.find(text=re.compile(r'\$[\d,]+'))
            price = self._parse_price(price_text) if price_text else 0

            # Try to extract other details
            text = listing.get_text()
            bedrooms = self._extract_number(text, r'(\d+)\s*bd')
            bathrooms = self._extract_number(text, r'(\d+\.?\d*)\s*ba')
            sqft = self._extract_number(text, r'([\d,]+)\s*sq')

            return Property(
                address=str(address).strip(),
                price=price,
                bedrooms=bedrooms or 0,
                bathrooms=bathrooms or 0,
                sqft=sqft or 0,
                year_built=0,
                property_type="auction",
                source="Real Broward County Auction",
                url=base_url,
                neighborhood="Fort Lauderdale",
                hoa_fees=0,
                description="Foreclosure Auction Property",
                listing_date=datetime.now().strftime("%Y-%m-%d")
            )
        except Exception as e:
            logging.warning(f"Error in _parse_auction_listing: {e}")
            return None

    def search_zillow_api(self) -> List[Property]:
        """Search for properties using Zillow-like search"""
        logging.info("Searching online listings...")
        properties = []

        # Note: This uses public search patterns. For production, consider using official APIs
        # Zillow, Realtor.com, Redfin all have different access methods

        search_urls = [
            "https://www.zillow.com/fort-lauderdale-fl/",
            "https://www.realtor.com/realestateandhomes-search/Fort-Lauderdale_FL",
        ]

        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
        }

        for url in search_urls:
            try:
                time.sleep(2)  # Be respectful with requests
                response = requests.get(url, headers=headers, timeout=30)

                if response.status_code == 200:
                    soup = BeautifulSoup(response.content, 'html.parser')
                    # This is a simplified parser - actual implementation needs specific selectors
                    logging.info(f"Successfully fetched {url}")

            except Exception as e:
                logging.error(f"Error searching {url}: {e}")

        return properties

    def search_public_records(self) -> List[Property]:
        """Search public MLS-style data sources"""
        logging.info("Searching public records and MLS sources...")
        properties = []

        # This would integrate with public data APIs
        # For example: Broward County Property Appraiser data

        try:
            # Broward County Property Appraiser has public search
            url = "https://web.bcpa.net/bcpaclient/#/Search"
            logging.info(f"Would search public records at {url}")
            # Implementation would go here

        except Exception as e:
            logging.error(f"Error searching public records: {e}")

        return properties

    def create_sample_properties(self) -> List[Property]:
        """Create sample properties for testing/demonstration"""
        logging.info("Creating sample properties for demonstration...")

        samples = [
            Property(
                address="123 Las Olas Blvd, Fort Lauderdale, FL 33301",
                price=450000,
                bedrooms=3,
                bathrooms=2.0,
                sqft=1800,
                year_built=2005,
                property_type="single-family",
                source="Sample Data",
                url="https://example.com/property1",
                neighborhood="Las Olas",
                hoa_fees=200,
                description="Beautiful waterfront property with updated kitchen",
                listing_date=datetime.now().strftime("%Y-%m-%d")
            ),
            Property(
                address="456 Victoria Park Rd, Fort Lauderdale, FL 33304",
                price=320000,
                bedrooms=2,
                bathrooms=2.0,
                sqft=1400,
                year_built=1985,
                property_type="condo",
                source="Sample Data",
                url="https://example.com/property2",
                neighborhood="Victoria Park",
                hoa_fees=350,
                description="Updated condo in prime location, pool and gym",
                listing_date=datetime.now().strftime("%Y-%m-%d")
            ),
            Property(
                address="789 Sailboat Dr, Fort Lauderdale, FL 33315",
                price=275000,
                bedrooms=3,
                bathrooms=2.5,
                sqft=1650,
                year_built=1978,
                property_type="townhouse",
                source="Sample Auction",
                url="https://example.com/auction1",
                neighborhood="Sailboat Bend",
                hoa_fees=150,
                description="Foreclosure auction - needs renovation, great bones",
                listing_date=datetime.now().strftime("%Y-%m-%d")
            ),
            Property(
                address="321 Sunrise Blvd, Fort Lauderdale, FL 33304",
                price=525000,
                bedrooms=4,
                bathrooms=3.0,
                sqft=2200,
                year_built=2010,
                property_type="single-family",
                source="Sample Data",
                url="https://example.com/property3",
                neighborhood="Sunrise East",
                hoa_fees=0,
                description="Modern home with pool, hurricane impact windows",
                listing_date=datetime.now().strftime("%Y-%m-%d")
            ),
        ]

        return samples

    def _matches_criteria(self, prop: Property) -> bool:
        """Check if property matches search criteria"""
        if prop.price < self.criteria.min_budget or prop.price > self.criteria.max_budget:
            return False

        if prop.bedrooms < self.criteria.min_bedrooms:
            return False

        if prop.bathrooms < self.criteria.min_bathrooms:
            return False

        if self.criteria.min_sqft > 0 and prop.sqft < self.criteria.min_sqft:
            return False

        if self.criteria.max_hoa > 0 and prop.hoa_fees > self.criteria.max_hoa:
            return False

        if self.criteria.property_types and prop.property_type not in self.criteria.property_types:
            return False

        return True

    def analyze_property(self, prop: Property) -> None:
        """Analyze property for investment potential and personal residence suitability"""
        score = 0.0
        reasons = []

        # Price analysis
        price_per_sqft = prop.price / prop.sqft if prop.sqft > 0 else 0

        # Fort Lauderdale average is around $300-400/sqft
        if price_per_sqft < 250:
            score += 25
            reasons.append(f"Excellent price/sqft: ${price_per_sqft:.0f}")
        elif price_per_sqft < 350:
            score += 15
            reasons.append(f"Good price/sqft: ${price_per_sqft:.0f}")
        elif price_per_sqft < 450:
            score += 5
            reasons.append(f"Fair price/sqft: ${price_per_sqft:.0f}")

        # Investment analysis
        if self.criteria.investment_focus:
            # Estimate rental income (rough estimate: $1.50-2.00 per sqft/month in FL)
            estimated_rent = prop.sqft * 1.75
            annual_rent = estimated_rent * 12

            # Calculate cap rate (simplified)
            # Assume 30% of rent goes to expenses (tax, insurance, HOA, maintenance)
            net_operating_income = annual_rent * 0.70 - (prop.hoa_fees * 12)
            cap_rate = (net_operating_income / prop.price * 100) if prop.price > 0 else 0

            if cap_rate > 8:
                score += 25
                reasons.append(f"Excellent cap rate: {cap_rate:.1f}%")
            elif cap_rate > 6:
                score += 15
                reasons.append(f"Good cap rate: {cap_rate:.1f}%")
            elif cap_rate > 4:
                score += 5
                reasons.append(f"Fair cap rate: {cap_rate:.1f}%")

            # 1% rule: monthly rent should be 1% of purchase price
            one_percent_value = prop.price * 0.01
            if estimated_rent >= one_percent_value:
                score += 15
                reasons.append("Meets 1% rule for cash flow")

        # Personal residence analysis
        else:
            # Location preferences
            if any(hood.lower() in prop.neighborhood.lower()
                   for hood in self.criteria.preferred_neighborhoods):
                score += 20
                reasons.append("In preferred neighborhood")

            # Property age
            age = datetime.now().year - prop.year_built if prop.year_built > 0 else 100
            if age < self.criteria.max_age_years:
                score += 15
                reasons.append(f"Property age ({age} years) meets requirements")

        # Common factors for both

        # Auction properties often have more potential
        if "auction" in prop.source.lower():
            score += 10
            reasons.append("Auction property - potential for below-market price")

        # HOA fees
        if prop.hoa_fees < 200:
            score += 10
            reasons.append(f"Low HOA fees: ${prop.hoa_fees:.0f}/month")
        elif prop.hoa_fees > 500:
            score -= 5
            reasons.append(f"High HOA fees: ${prop.hoa_fees:.0f}/month")

        # Size appropriateness
        if prop.bedrooms >= self.criteria.min_bedrooms + 1:
            score += 5
            reasons.append("Extra bedrooms")

        prop.analysis_score = score

        # Generate recommendation
        if score >= 50:
            prop.recommendation = "🌟 EXCELLENT OPPORTUNITY - " + "; ".join(reasons)
        elif score >= 35:
            prop.recommendation = "✓ GOOD OPPORTUNITY - " + "; ".join(reasons)
        elif score >= 20:
            prop.recommendation = "Consider - " + "; ".join(reasons)
        else:
            prop.recommendation = "Review carefully - " + "; ".join(reasons)

    def search_all_sources(self) -> List[Property]:
        """Search all available sources"""
        all_properties = []

        # Search Real Broward auctions
        all_properties.extend(self.search_real_broward_auctions())

        # Search online listings
        all_properties.extend(self.search_zillow_api())

        # Search public records
        all_properties.extend(self.search_public_records())

        # Add sample properties for demonstration
        all_properties.extend(self.create_sample_properties())

        # Filter by criteria
        matching_properties = [p for p in all_properties if self._matches_criteria(p)]

        # Analyze each property
        for prop in matching_properties:
            self.analyze_property(prop)
            self.mark_property_seen(prop.address, prop.price)

        # Sort by analysis score
        matching_properties.sort(key=lambda p: p.analysis_score, reverse=True)

        self.properties = matching_properties
        return matching_properties

    def _parse_price(self, text: str) -> float:
        """Extract price from text"""
        if not text:
            return 0.0
        numbers = re.findall(r'[\d,]+', str(text))
        if numbers:
            return float(numbers[0].replace(',', ''))
        return 0.0

    def _extract_number(self, text: str, pattern: str) -> float:
        """Extract number matching pattern from text"""
        match = re.search(pattern, text, re.I)
        if match:
            return float(match.group(1).replace(',', ''))
        return 0.0

    def send_email_report(self, smtp_server: str = None, smtp_port: int = None,
                          sender_email: str = None, sender_password: str = None):
        """Send email report with findings"""

        if not self.properties:
            logging.info("No properties to report")
            return

        # Use environment variables if parameters not provided
        smtp_server = smtp_server or os.getenv('SMTP_SERVER', 'smtp.gmail.com')
        smtp_port = smtp_port or int(os.getenv('SMTP_PORT', '587'))
        sender_email = sender_email or os.getenv('SENDER_EMAIL')
        sender_password = sender_password or os.getenv('SENDER_PASSWORD')

        if not sender_email or not sender_password:
            logging.warning("Email credentials not configured. Saving report to file instead.")
            self.save_report_to_file()
            return

        # Create email
        msg = MIMEMultipart('alternative')
        msg['Subject'] = f'Fort Lauderdale House Search Results - {datetime.now().strftime("%Y-%m-%d")}'
        msg['From'] = sender_email
        msg['To'] = self.criteria.email

        # Create HTML report
        html_body = self._generate_html_report()

        # Attach HTML
        html_part = MIMEText(html_body, 'html')
        msg.attach(html_part)

        # Send email
        try:
            logging.info(f"Sending email to {self.criteria.email}...")
            with smtplib.SMTP(smtp_server, smtp_port) as server:
                server.starttls()
                server.login(sender_email, sender_password)
                server.send_message(msg)
            logging.info("Email sent successfully!")
        except Exception as e:
            logging.error(f"Error sending email: {e}")
            logging.info("Saving report to file instead...")
            self.save_report_to_file()

    def _generate_html_report(self) -> str:
        """Generate HTML email report"""
        html = f"""
        <html>
        <head>
            <style>
                body {{ font-family: Arial, sans-serif; }}
                .header {{ background-color: #2c3e50; color: white; padding: 20px; }}
                .property {{ border: 1px solid #ddd; margin: 20px; padding: 15px; }}
                .excellent {{ border-left: 5px solid #27ae60; }}
                .good {{ border-left: 5px solid #3498db; }}
                .price {{ font-size: 24px; font-weight: bold; color: #27ae60; }}
                .address {{ font-size: 18px; color: #2c3e50; }}
                .details {{ margin: 10px 0; }}
                .recommendation {{ background-color: #ecf0f1; padding: 10px; margin: 10px 0; }}
                .score {{ font-weight: bold; color: #e74c3c; }}
            </style>
        </head>
        <body>
            <div class="header">
                <h1>Fort Lauderdale Real Estate Search Results</h1>
                <p>Date: {datetime.now().strftime("%B %d, %Y")}</p>
                <p>Found {len(self.properties)} properties matching your criteria</p>
            </div>
        """

        for prop in self.properties[:20]:  # Top 20 properties
            css_class = "excellent" if prop.analysis_score >= 50 else "good" if prop.analysis_score >= 35 else ""

            html += f"""
            <div class="property {css_class}">
                <div class="address">{prop.address}</div>
                <div class="price">${prop.price:,.0f}</div>
                <div class="details">
                    <strong>{prop.bedrooms} bed</strong> |
                    <strong>{prop.bathrooms} bath</strong> |
                    <strong>{prop.sqft:,} sqft</strong> |
                    Built: {prop.year_built if prop.year_built else 'N/A'}
                </div>
                <div class="details">
                    Type: {prop.property_type} |
                    HOA: ${prop.hoa_fees:.0f}/mo |
                    Source: {prop.source}
                </div>
                <div class="details">
                    Neighborhood: {prop.neighborhood}
                </div>
                <div class="recommendation">
                    <div class="score">Score: {prop.analysis_score:.0f}/100</div>
                    {prop.recommendation}
                </div>
                <div>
                    <a href="{prop.url}">View Listing</a>
                </div>
            </div>
            """

        html += """
            <div style="margin: 20px; padding: 15px; background-color: #ecf0f1;">
                <h3>Search Criteria Used:</h3>
                <p>Budget: ${:,.0f} - ${:,.0f}</p>
                <p>Bedrooms: {} min | Bathrooms: {} min</p>
                <p>Square Feet: {:,} min</p>
                <p>Focus: {}</p>
            </div>
        </body>
        </html>
        """.format(
            self.criteria.min_budget,
            self.criteria.max_budget,
            self.criteria.min_bedrooms,
            self.criteria.min_bathrooms,
            self.criteria.min_sqft,
            "Investment Property" if self.criteria.investment_focus else "Personal Residence"
        )

        return html

    def save_report_to_file(self):
        """Save report to HTML file"""
        filename = f"house_search_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.html"
        html = self._generate_html_report()

        with open(filename, 'w') as f:
            f.write(html)

        logging.info(f"Report saved to {filename}")


def get_user_criteria() -> SearchCriteria:
    """Get search criteria from user input"""
    print("\n" + "="*60)
    print("FORT LAUDERDALE REAL ESTATE SEARCH & ANALYSIS")
    print("="*60 + "\n")

    print("Let's set up your search criteria...\n")

    # Budget
    while True:
        try:
            min_budget = float(input("Minimum budget ($): ").replace(',', '').replace('$', ''))
            max_budget = float(input("Maximum budget ($): ").replace(',', '').replace('$', ''))
            if min_budget < max_budget:
                break
            print("Maximum budget must be greater than minimum!")
        except ValueError:
            print("Please enter valid numbers")

    # Bedrooms and bathrooms
    while True:
        try:
            min_bedrooms = int(input("Minimum bedrooms: "))
            min_bathrooms = float(input("Minimum bathrooms: "))
            break
        except ValueError:
            print("Please enter valid numbers")

    # Square footage
    while True:
        try:
            min_sqft = int(input("Minimum square feet (0 for any): ").replace(',', ''))
            break
        except ValueError:
            print("Please enter a valid number")

    # Property types
    print("\nProperty types (enter comma-separated):")
    print("Options: single-family, condo, townhouse, multi-family")
    types_input = input("Property types (or press Enter for all): ").strip()
    if types_input:
        property_types = [t.strip() for t in types_input.split(',')]
    else:
        property_types = []

    # Neighborhoods
    print("\nPreferred neighborhoods in Fort Lauderdale (comma-separated):")
    print("Examples: Las Olas, Victoria Park, Sailboat Bend, Coral Ridge, Rio Vista")
    neighborhoods_input = input("Neighborhoods (or press Enter for all): ").strip()
    if neighborhoods_input:
        neighborhoods = [n.strip() for n in neighborhoods_input.split(',')]
    else:
        neighborhoods = []

    # HOA
    while True:
        try:
            max_hoa = float(input("Maximum monthly HOA fees (0 for any): ").replace(',', '').replace('$', ''))
            break
        except ValueError:
            print("Please enter a valid number")

    # Property age
    while True:
        try:
            max_age = int(input("Maximum property age in years (0 for any): "))
            break
        except ValueError:
            print("Please enter a valid number")

    # Investment focus
    focus_input = input("\nPrimary focus - Investment or Personal? (i/p): ").strip().lower()
    investment_focus = focus_input.startswith('i')

    # Email
    email = input("\nYour email address for reports: ").strip()

    return SearchCriteria(
        max_budget=max_budget,
        min_budget=min_budget,
        min_bedrooms=min_bedrooms,
        min_bathrooms=min_bathrooms,
        preferred_neighborhoods=neighborhoods,
        property_types=property_types,
        investment_focus=investment_focus,
        max_age_years=max_age,
        min_sqft=min_sqft,
        max_hoa=max_hoa,
        email=email
    )


def save_criteria(criteria: SearchCriteria):
    """Save criteria to file for future runs"""
    with open('search_criteria.json', 'w') as f:
        json.dump(asdict(criteria), f, indent=2)
    logging.info("Criteria saved to search_criteria.json")


def load_criteria() -> SearchCriteria:
    """Load criteria from file"""
    try:
        with open('search_criteria.json', 'r') as f:
            data = json.load(f)
            return SearchCriteria(**data)
    except FileNotFoundError:
        return None


def main():
    """Main function"""
    print("\n" + "="*60)
    print("FORT LAUDERDALE HOUSE SEARCH")
    print("="*60 + "\n")

    # Check if we have saved criteria
    criteria = load_criteria()
    if criteria:
        use_saved = input("Use saved search criteria? (y/n): ").strip().lower()
        if not use_saved.startswith('y'):
            criteria = get_user_criteria()
            save_criteria(criteria)
    else:
        criteria = get_user_criteria()
        save_criteria(criteria)

    print("\n" + "-"*60)
    print("Starting search...")
    print("-"*60 + "\n")

    # Create searcher and run search
    searcher = RealEstateSearcher(criteria)
    properties = searcher.search_all_sources()

    print(f"\n✓ Found {len(properties)} properties matching your criteria!")

    if properties:
        print("\nTop 5 properties:\n")
        for i, prop in enumerate(properties[:5], 1):
            print(f"{i}. {prop.address}")
            print(f"   ${prop.price:,.0f} | {prop.bedrooms}bd {prop.bathrooms}ba | {prop.sqft:,} sqft")
            print(f"   Score: {prop.analysis_score:.0f}/100")
            print(f"   {prop.recommendation}\n")

    # Send email report
    print("\nGenerating and sending email report...")
    searcher.send_email_report()

    print("\n" + "="*60)
    print("Search complete! Check your email for the full report.")
    print("="*60 + "\n")


if __name__ == "__main__":
    main()
