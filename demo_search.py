#!/usr/bin/env python3
"""
Demo script to show the house search tool in action
Runs with pre-configured criteria - no user input needed
"""

from fort_lauderdale_house_search import RealEstateSearcher, SearchCriteria
import logging

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

def main():
    print("\n" + "="*70)
    print("FORT LAUDERDALE HOUSE SEARCH - DEMO MODE")
    print("="*70 + "\n")

    # Pre-configured demo criteria
    demo_criteria = SearchCriteria(
        max_budget=500000,
        min_budget=250000,
        min_bedrooms=3,
        min_bathrooms=2.0,
        preferred_neighborhoods=["Las Olas", "Victoria Park", "Sailboat Bend"],
        property_types=["single-family", "townhouse", "condo"],
        investment_focus=True,  # Looking for investment properties
        max_age_years=50,
        min_sqft=1400,
        max_hoa=400,
        email="demo@example.com"
    )

    print("Demo Search Criteria:")
    print(f"  Budget: ${demo_criteria.min_budget:,} - ${demo_criteria.max_budget:,}")
    print(f"  Bedrooms: {demo_criteria.min_bedrooms}+ | Bathrooms: {demo_criteria.min_bathrooms}+")
    print(f"  Min Square Feet: {demo_criteria.min_sqft:,}")
    print(f"  Max HOA: ${demo_criteria.max_hoa}/month")
    print(f"  Property Types: {', '.join(demo_criteria.property_types)}")
    print(f"  Preferred Areas: {', '.join(demo_criteria.preferred_neighborhoods)}")
    print(f"  Focus: Investment Properties")
    print("\n" + "-"*70 + "\n")

    # Create searcher
    searcher = RealEstateSearcher(demo_criteria)

    # Run search
    print("Searching for properties...\n")
    properties = searcher.search_all_sources()

    # Display results
    if not properties:
        print("No properties found matching criteria.")
        return

    print(f"✓ Found {len(properties)} properties!\n")
    print("="*70)
    print("TOP OPPORTUNITIES")
    print("="*70 + "\n")

    # Show top 10
    for i, prop in enumerate(properties[:10], 1):
        print(f"{i}. {prop.address}")
        print(f"   {'─'*66}")
        print(f"   Price: ${prop.price:,.0f} | {prop.bedrooms}bd {prop.bathrooms}ba | {prop.sqft:,} sqft")
        print(f"   Built: {prop.year_built if prop.year_built else 'N/A'} | Type: {prop.property_type}")
        print(f"   HOA: ${prop.hoa_fees:.0f}/mo | Source: {prop.source}")

        # Calculate metrics
        price_per_sqft = prop.price / prop.sqft if prop.sqft > 0 else 0
        print(f"   Price/sqft: ${price_per_sqft:.0f}")

        # Investment metrics
        if demo_criteria.investment_focus:
            estimated_rent = prop.sqft * 1.75
            annual_rent = estimated_rent * 12
            net_operating_income = annual_rent * 0.70 - (prop.hoa_fees * 12)
            cap_rate = (net_operating_income / prop.price * 100) if prop.price > 0 else 0

            print(f"   Est. Rent: ${estimated_rent:.0f}/mo | Cap Rate: {cap_rate:.1f}%")

        print(f"\n   📊 ANALYSIS SCORE: {prop.analysis_score:.0f}/100")
        print(f"   {prop.recommendation}")
        print()

    # Save report
    print("="*70)
    print("\nGenerating HTML report...")
    searcher.save_report_to_file()
    print("\n✓ Demo complete! Check the generated HTML report for full details.\n")

    print("="*70)
    print("To run with your own criteria: python3 fort_lauderdale_house_search.py")
    print("="*70 + "\n")


if __name__ == "__main__":
    main()
