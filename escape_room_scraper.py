#!/usr/bin/env python3
"""
Escape Room Industry News Scraper
Scrapes escape room industry websites for new articles and sends email notifications with feedback options.
"""

import json
import os
import hashlib
import requests
from bs4 import BeautifulSoup
from datetime import datetime, timedelta
from typing import List, Dict, Optional
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.utils import formataddr
import time
import re
from pathlib import Path


def load_env_file(env_path='.env'):
    """Load environment variables from .env file."""
    env_file = Path(env_path)
    if not env_file.exists():
        return {}

    env_vars = {}
    with open(env_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue

            # Parse KEY=VALUE
            if '=' in line:
                key, value = line.split('=', 1)
                env_vars[key.strip()] = value.strip()

    return env_vars


class EscapeRoomNewsScraper:
    def __init__(self, config_file='escape_room_sources.json', cache_file='seen_articles.json'):
        self.config_file = config_file
        self.cache_file = cache_file
        self.sources = self.load_sources()
        self.seen_articles = self.load_seen_articles()
        self.new_articles = []

    def load_sources(self) -> List[Dict]:
        """Load news sources from configuration file."""
        try:
            with open(self.config_file, 'r') as f:
                data = json.load(f)
                return data.get('sources', [])
        except FileNotFoundError:
            print(f"Error: Configuration file '{self.config_file}' not found.")
            return []
        except json.JSONDecodeError:
            print(f"Error: Invalid JSON in '{self.config_file}'.")
            return []

    def load_seen_articles(self) -> Dict:
        """Load previously seen articles from cache."""
        if os.path.exists(self.cache_file):
            try:
                with open(self.cache_file, 'r') as f:
                    return json.load(f)
            except json.JSONDecodeError:
                return {}
        return {}

    def save_seen_articles(self):
        """Save seen articles to cache file."""
        with open(self.cache_file, 'w') as f:
            json.dump(self.seen_articles, f, indent=2)

    def generate_article_id(self, title: str, url: str) -> str:
        """Generate unique ID for an article based on title and URL."""
        content = f"{title}|{url}"
        return hashlib.md5(content.encode()).hexdigest()

    def scrape_generic_blog(self, source: Dict) -> List[Dict]:
        """Generic blog scraper that attempts to find articles on any blog/news site."""
        articles = []
        try:
            headers = {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
            }

            response = requests.get(source['url'], headers=headers, timeout=10)
            response.raise_for_status()

            soup = BeautifulSoup(response.content, 'html.parser')

            # Try multiple common article patterns
            article_selectors = [
                'article',
                '.post',
                '.entry',
                '.blog-post',
                '.news-item',
                '.article',
                '[class*="post"]',
                '[class*="article"]',
            ]

            found_articles = []
            for selector in article_selectors:
                found = soup.select(selector)
                if found:
                    found_articles.extend(found)
                    break

            # If no articles found with selectors, look for links with article-like patterns
            if not found_articles:
                # Look for links in main content area
                main_content = soup.find(['main', 'div[class*="content"]', 'div[class*="main"]'])
                if main_content:
                    links = main_content.find_all('a', href=True)
                else:
                    links = soup.find_all('a', href=True)

                # Filter for likely article links
                for link in links[:20]:  # Limit to first 20 links
                    href = link.get('href', '')
                    text = link.get_text(strip=True)

                    # Skip if too short or looks like navigation
                    if len(text) < 15 or not href:
                        continue

                    # Skip navigation items
                    skip_keywords = ['home', 'about', 'contact', 'privacy', 'terms', 'menu', 'login', 'register']
                    if any(keyword in text.lower() for keyword in skip_keywords):
                        continue

                    # Make URL absolute
                    if href.startswith('/'):
                        from urllib.parse import urljoin
                        href = urljoin(source['url'], href)
                    elif not href.startswith('http'):
                        continue

                    articles.append({
                        'title': text,
                        'url': href,
                        'source': source['name'],
                        'date': datetime.now().strftime('%Y-%m-%d')
                    })
            else:
                # Process found article elements
                for article_elem in found_articles[:15]:  # Limit to 15 most recent
                    # Try to find title
                    title_elem = article_elem.find(['h1', 'h2', 'h3', 'h4', 'a'])
                    if not title_elem:
                        continue

                    title = title_elem.get_text(strip=True)

                    # Try to find link
                    link_elem = article_elem.find('a', href=True)
                    if not link_elem:
                        link_elem = title_elem if title_elem.name == 'a' else None

                    if not link_elem:
                        continue

                    url = link_elem.get('href', '')

                    # Make URL absolute
                    if url.startswith('/'):
                        from urllib.parse import urljoin
                        url = urljoin(source['url'], url)
                    elif not url.startswith('http'):
                        continue

                    # Try to find date
                    date_elem = article_elem.find(['time', 'span[class*="date"]', 'div[class*="date"]'])
                    article_date = date_elem.get_text(strip=True) if date_elem else datetime.now().strftime('%Y-%m-%d')

                    articles.append({
                        'title': title,
                        'url': url,
                        'source': source['name'],
                        'date': article_date
                    })

        except requests.RequestException as e:
            print(f"Error scraping {source['name']}: {e}")
        except Exception as e:
            print(f"Unexpected error scraping {source['name']}: {e}")

        return articles

    def scrape_all_sources(self):
        """Scrape all configured sources for new articles."""
        print(f"Scraping {len(self.sources)} sources...")

        for source in self.sources:
            print(f"\nScraping: {source['name']}...")
            articles = self.scrape_generic_blog(source)

            # Filter out already seen articles
            for article in articles:
                article_id = self.generate_article_id(article['title'], article['url'])

                if article_id not in self.seen_articles:
                    self.new_articles.append(article)
                    self.seen_articles[article_id] = {
                        'title': article['title'],
                        'url': article['url'],
                        'source': article['source'],
                        'first_seen': datetime.now().isoformat()
                    }

            print(f"  Found {len(articles)} articles, {sum(1 for a in articles if self.generate_article_id(a['title'], a['url']) not in self.seen_articles)} are new")

            # Be respectful - add delay between requests
            time.sleep(2)

        print(f"\nTotal new articles found: {len(self.new_articles)}")
        return self.new_articles

    def generate_feedback_url(self, article_id: str, feedback_type: str) -> str:
        """Generate a feedback URL (in production, this would link to a web service)."""
        # This is a placeholder - in production, you'd set up a web endpoint to handle these
        base_url = "http://localhost:5000/feedback"
        return f"{base_url}?article={article_id}&feedback={feedback_type}"

    def create_email_body(self, articles: List[Dict], recipient_email: str) -> str:
        """Create HTML email body with articles and feedback options."""

        html = """
        <html>
        <head>
            <style>
                body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; }
                .header { background-color: #4CAF50; color: white; padding: 20px; text-align: center; }
                .article { border-left: 4px solid #4CAF50; padding: 15px; margin: 20px 0; background-color: #f9f9f9; }
                .article h3 { margin-top: 0; color: #2c3e50; }
                .article a { color: #3498db; text-decoration: none; }
                .article a:hover { text-decoration: underline; }
                .source { color: #7f8c8d; font-size: 0.9em; }
                .date { color: #95a5a6; font-size: 0.85em; }
                .feedback { margin-top: 10px; padding: 10px; background-color: #ecf0f1; border-radius: 5px; }
                .feedback-link { display: inline-block; margin: 5px 10px 5px 0; padding: 8px 15px; background-color: #3498db; color: white; text-decoration: none; border-radius: 3px; font-size: 0.9em; }
                .feedback-link:hover { background-color: #2980b9; }
                .feedback-link.dislike { background-color: #e74c3c; }
                .feedback-link.dislike:hover { background-color: #c0392b; }
                .footer { margin-top: 30px; padding: 20px; background-color: #34495e; color: white; }
                .footer textarea { width: 100%; height: 80px; margin: 10px 0; padding: 10px; border-radius: 5px; border: none; }
                .footer input { width: 100%; padding: 10px; margin: 5px 0; border-radius: 5px; border: none; }
                .summary { padding: 15px; background-color: #e8f5e9; margin: 20px 0; border-radius: 5px; }
            </style>
        </head>
        <body>
            <div class="header">
                <h1>🔐 Escape Room Industry News Update</h1>
                <p>Your latest escape room industry news digest</p>
            </div>

            <div class="summary">
                <strong>📊 Summary:</strong> Found {article_count} new article(s) from {source_count} source(s)
                <br><strong>📅 Date:</strong> {date}
            </div>
        """.format(
            article_count=len(articles),
            source_count=len(set(a['source'] for a in articles)),
            date=datetime.now().strftime('%B %d, %Y')
        )

        # Group articles by source
        articles_by_source = {}
        for article in articles:
            source = article['source']
            if source not in articles_by_source:
                articles_by_source[source] = []
            articles_by_source[source].append(article)

        # Add articles grouped by source
        for source, source_articles in articles_by_source.items():
            html += f'<h2 style="color: #2c3e50; border-bottom: 2px solid #4CAF50; padding-bottom: 10px;">📰 {source}</h2>'

            for article in source_articles:
                article_id = self.generate_article_id(article['title'], article['url'])

                html += f"""
                <div class="article">
                    <h3><a href="{article['url']}" target="_blank">{article['title']}</a></h3>
                    <div class="source">Source: {article['source']}</div>
                    <div class="date">Date: {article['date']}</div>

                    <div class="feedback">
                        <strong>Was this article useful?</strong><br>
                        <a href="mailto:{recipient_email}?subject=Feedback: Liked Article&body=I liked this article: {article['title']}%0A{article['url']}%0A%0AArticle ID: {article_id}" class="feedback-link">👍 Yes, I liked it</a>
                        <a href="mailto:{recipient_email}?subject=Feedback: Disliked Article&body=I did not find this article useful: {article['title']}%0A{article['url']}%0A%0AArticle ID: {article_id}" class="feedback-link dislike">👎 Not relevant</a>
                    </div>
                </div>
                """

        # Add feedback section
        html += """
            <div class="footer">
                <h2>💬 Your Feedback Matters!</h2>
                <p>Help us improve your news feed:</p>

                <h3>Add New Sources</h3>
                <p>Know of other great escape room industry websites? Send us the URLs:</p>
                <p><a href="mailto:{email}?subject=New Source Suggestions&body=Please add these websites to monitor:%0A%0A1. [Website URL]%0A2. [Website URL]%0A3. [Website URL]%0A%0A" style="color: #3498db;">Click here to suggest new sources</a></p>

                <h3>Remove Sources</h3>
                <p>Not interested in certain sources? Let us know:</p>
                <p><a href="mailto:{email}?subject=Remove Sources&body=Please stop monitoring these sources:%0A%0A1. [Source Name]%0A2. [Source Name]%0A%0A" style="color: #3498db;">Click here to remove sources</a></p>

                <h3>General Feedback</h3>
                <p><a href="mailto:{email}?subject=News Scraper Feedback&body=Your feedback here..." style="color: #3498db;">Send us your thoughts and suggestions</a></p>

                <hr style="margin: 20px 0; border: none; border-top: 1px solid #7f8c8d;">
                <p style="font-size: 0.9em; color: #bdc3c7;">
                    You're receiving this because you subscribed to Escape Room Industry News updates.<br>
                    Scraper running on: {hostname}<br>
                    Next update: Check schedule in configuration
                </p>
            </div>
        </body>
        </html>
        """.format(email=recipient_email, hostname=os.uname().nodename)

        return html

    def send_email(self, recipient_email: str, smtp_server: str, smtp_port: int,
                   sender_email: str, sender_password: str, sender_name: str = "Escape Room News Bot"):
        """Send email with new articles and feedback options."""

        if not self.new_articles:
            print("No new articles to send.")
            return False

        try:
            # Create message
            msg = MIMEMultipart('alternative')
            msg['Subject'] = f"🔐 Escape Room News Update - {len(self.new_articles)} New Article(s)"
            msg['From'] = formataddr((sender_name, sender_email))
            msg['To'] = recipient_email

            # Create HTML body
            html_body = self.create_email_body(self.new_articles, recipient_email)

            # Create plain text version
            text_body = f"Escape Room Industry News Update\n\n"
            text_body += f"Found {len(self.new_articles)} new articles:\n\n"

            for article in self.new_articles:
                text_body += f"Title: {article['title']}\n"
                text_body += f"Source: {article['source']}\n"
                text_body += f"URL: {article['url']}\n"
                text_body += f"Date: {article['date']}\n\n"

            text_body += f"\nTo provide feedback, reply to this email.\n"

            # Attach both versions
            part1 = MIMEText(text_body, 'plain')
            part2 = MIMEText(html_body, 'html')
            msg.attach(part1)
            msg.attach(part2)

            # Send email
            print(f"\nSending email to {recipient_email}...")

            with smtplib.SMTP(smtp_server, smtp_port) as server:
                server.starttls()
                server.login(sender_email, sender_password)
                server.send_message(msg)

            print("Email sent successfully!")
            return True

        except Exception as e:
            print(f"Error sending email: {e}")
            return False

    def run(self, send_email_flag=False, **email_config):
        """Run the scraper and optionally send email."""
        print("=" * 60)
        print("Escape Room Industry News Scraper")
        print("=" * 60)

        # Scrape all sources
        self.scrape_all_sources()

        # Save seen articles
        self.save_seen_articles()

        # Send email if requested and there are new articles
        if send_email_flag and self.new_articles and email_config:
            self.send_email(**email_config)

        return self.new_articles


def main():
    """Main function to run the scraper."""

    # Load environment variables
    env_vars = load_env_file('.env')

    if not env_vars:
        print("WARNING: No .env file found. Please copy .env.example to .env and configure it.")
        print("You can still run the scraper, but email notifications will be disabled.\n")

    # Initialize scraper
    scraper = EscapeRoomNewsScraper()

    # Check if we should send email
    send_email_flag = env_vars.get('SEND_EMAIL', 'false').lower() == 'true'

    # Email configuration from environment
    EMAIL_CONFIG = {
        'recipient_email': env_vars.get('RECIPIENT_EMAIL', 'your-email@example.com'),
        'sender_email': env_vars.get('SENDER_EMAIL', 'your-sender-email@gmail.com'),
        'sender_password': env_vars.get('SENDER_PASSWORD', ''),
        'sender_name': env_vars.get('SENDER_NAME', 'Escape Room News Bot'),
        'smtp_server': env_vars.get('SMTP_SERVER', 'smtp.gmail.com'),
        'smtp_port': int(env_vars.get('SMTP_PORT', 587))
    }

    # Validate email configuration if sending email
    if send_email_flag:
        if not all([EMAIL_CONFIG['recipient_email'] != 'your-email@example.com',
                    EMAIL_CONFIG['sender_email'] != 'your-sender-email@gmail.com',
                    EMAIL_CONFIG['sender_password']]):
            print("ERROR: Email configuration is incomplete. Please configure .env file.")
            print("Email sending will be disabled for this run.\n")
            send_email_flag = False

    # Run scraper
    new_articles = scraper.run(send_email_flag=send_email_flag, **EMAIL_CONFIG)

    # Print summary
    print("\n" + "=" * 60)
    print(f"Scraping complete! Found {len(new_articles)} new articles.")
    print("=" * 60)

    if new_articles:
        print("\nNew Articles:")
        for i, article in enumerate(new_articles, 1):
            print(f"\n{i}. {article['title']}")
            print(f"   Source: {article['source']}")
            print(f"   URL: {article['url']}")
    else:
        print("\nNo new articles found. All articles have been seen before.")


if __name__ == "__main__":
    main()
