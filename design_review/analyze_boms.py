#!/usr/bin/env python3
"""Analyze BOMs to compute total price and count of distinct extended parts.

Usage:
  python analyze_boms.py              # Use prices from BOM CSVs
  python analyze_boms.py --fetch      # Fetch live prices from JLCPcb API
  python analyze_boms.py --live       # Fetch live prices from JLCPcb API (alias)
"""

import csv
import sys
import json
import time
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import URLError


def fetch_jlcpcb_prices(part_numbers, delay=0.1):
    """
    Fetch live prices and flags from jlcsearch API for LCSC/JLCPCB part numbers.

    Args:
        part_numbers: list of LCSC part numbers (e.g., ["C123456", "R123456"])
        delay: seconds to wait between requests to avoid rate limiting

    Returns:
        dict mapping lcsc_code -> (price, is_preferred) tuple
    """
    if not part_numbers:
        return {}

    prices = {}
    api_base = "https://jlcsearch.tscircuit.com/components/list.json"

    for part_code in part_numbers:
        try:
            # Query jlcsearch API for this part
            url = f"{api_base}?search={part_code}"
            req = Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urlopen(req, timeout=10) as response:
                data = json.loads(response.read().decode('utf-8'))

                # Find the matching part in results
                for component in data.get('components', []):
                    # Extract numeric part from LCSC code (e.g., "C10209" -> 10209)
                    lcsc_num = int(part_code[1:]) if part_code and part_code[0].isalpha() else int(part_code)

                    if component.get('lcsc') == lcsc_num:
                        price_data = component.get('price', '[]')
                        is_preferred = component.get('is_preferred', False)
                        try:
                            # Price is a JSON string with quantity tiers
                            price_tiers = json.loads(price_data) if isinstance(price_data, str) else price_data
                            if price_tiers and len(price_tiers) > 0:
                                # Use lowest quantity tier price (typically better for small orders)
                                price = float(price_tiers[0].get('price', 0))
                                prices[part_code] = (price, is_preferred)
                        except (ValueError, TypeError, json.JSONDecodeError):
                            pass
                        break
        except (URLError, json.JSONDecodeError, Exception):
            # Continue with next part even if one fails
            pass

        time.sleep(delay)

    return prices


def analyze_bom(filepath, fetch_live_prices=False):
    """
    Analyze a single BOM file.
    Returns: (total_price, num_basic_parts, num_promotional_extended_parts, num_extended_parts, num_missing_price)
    """
    total_price = 0.0
    basic_parts = {}
    promotional_extended_parts = {}
    extended_parts = {}
    missing_price_parts = set()
    all_parts = []  # List of (supplier_part, quantity, price, part_class, comment)
    live_prices = {}

    # First pass: collect all parts and prices from CSV
    # BOMs are UTF-16 encoded
    with open(filepath, 'r', encoding='utf-16') as f:
        reader = csv.DictReader(f, delimiter='\t')

        for row in reader:
            try:
                quantity = int(row.get('Quantity', '0'))
                price_str = row.get('JLCPCB Price', '0')
                price = float(price_str) if price_str else 0.0
                supplier_part = row.get('Supplier Part', '').strip()
                part_class = row.get('JLCPCB Part Class', '')
                comment = row.get('Comment', '').strip()

                if supplier_part:
                    all_parts.append((supplier_part, quantity, price, part_class, comment))

            except (ValueError, KeyError) as e:
                print(f"Warning: Error parsing row in {filepath.name}: {e}", file=sys.stderr)
                continue

    # Fetch live prices if requested
    if fetch_live_prices:
        unique_parts = list(set(part[0] for part in all_parts))
        if unique_parts:
            print(f"Fetching live prices for {len(unique_parts)} unique parts from {filepath.name}...", file=sys.stderr)
            live_prices = fetch_jlcpcb_prices(unique_parts)

    # Second pass: calculate totals using fetched or CSV prices
    for supplier_part, quantity, csv_price, part_class, comment in all_parts:
        # Check if we have live price data
        if supplier_part in live_prices:
            price, is_preferred = live_prices[supplier_part]
        else:
            price = csv_price
            is_preferred = False

        # Accumulate total price
        total_price += quantity * price

        # Track parts by category with their quantity, comment and price
        part_info = {'quantity': quantity, 'comment': comment, 'price': price}

        if part_class.strip() == 'Basic Part':
            basic_parts[supplier_part] = part_info
        elif part_class.strip() == 'Extended Part':
            if is_preferred:
                promotional_extended_parts[supplier_part] = part_info
            else:
                extended_parts[supplier_part] = part_info
            # Track missing price for extended parts
            if price == 0.0:
                missing_price_parts.add(supplier_part)

    return total_price, len(basic_parts), len(promotional_extended_parts), len(extended_parts), len(missing_price_parts), basic_parts, promotional_extended_parts, extended_parts, missing_price_parts


def main():
    """Process all BOM files in the export directory."""
    fetch_live_prices = '--fetch' in sys.argv or '--live' in sys.argv

    export_dir = Path(__file__).parent / 'export'

    if not export_dir.exists():
        print(f"Error: export directory not found at {export_dir}")
        sys.exit(1)

    # Find all BOM CSV files
    bom_files = sorted(export_dir.glob('BOM_*.csv'))

    if not bom_files:
        print(f"No BOM files found in {export_dir}")
        sys.exit(1)

    if fetch_live_prices:
        print("Fetching live prices from JLCPcb...")
        print()

    print(f"{'BOM Name':<40} {'Total Price':>15} {'# Basic':>8} {'# Promo Ext':>11} {'# Extended':>11} {'# No Price':>11}")
    print("-" * 109)

    total_all = 0.0
    total_basic_all = 0
    total_promo_ext_all = 0
    total_extended_all = 0
    total_missing_all = 0
    all_basic_parts = {}
    all_promo_ext_parts = {}
    all_extended_parts = {}
    all_missing_parts = set()

    results = []
    for bom_file in bom_files:
        price, num_basic, num_promo_ext, num_extended, num_missing, basic_refs, promo_ext_refs, extended_refs, missing_refs = analyze_bom(bom_file, fetch_live_prices=fetch_live_prices)
        results.append((bom_file.stem, price, num_basic, num_promo_ext, num_extended, num_missing, basic_refs, promo_ext_refs, extended_refs, missing_refs))
        total_all += price
        total_basic_all += num_basic
        total_promo_ext_all += num_promo_ext
        total_extended_all += num_extended
        total_missing_all += num_missing
        all_basic_parts.update(basic_refs)
        all_promo_ext_parts.update(promo_ext_refs)
        all_extended_parts.update(extended_refs)
        all_missing_parts.update(missing_refs)

        print(f"{bom_file.stem:<40} ${price:>14.2f} {num_basic:>8} {num_promo_ext:>10} {num_extended:>10} {num_missing:>10}")

    print("-" * 109)
    print(f"{'TOTAL':<40} ${total_all:>14.2f} {total_basic_all:>8} {total_promo_ext_all:>11} {total_extended_all:>11} {total_missing_all:>11}")
    print(f"{'DISTINCT ACROSS BOARDS':<40} {'':<15} {len(all_basic_parts):>8} {len(all_promo_ext_parts):>11} {len(all_extended_parts):>11} {len(all_missing_parts):>11}")
    print()

    # Detailed parts report
    print("\nDetailed Parts Report:")
    print("=" * 150)

    for bom_name, price, num_basic, num_promo_ext, num_extended, num_missing, basic_refs, promo_ext_refs, extended_refs, missing_refs in results:
        print(f"\n{bom_name}:")

        if extended_refs:
            print(f"  Extended parts ({len(extended_refs)} total):")
            # Sort by total price descending (price × quantity)
            sorted_parts = sorted(extended_refs.items(), key=lambda x: x[1]['price'] * x[1]['quantity'], reverse=True)
            for ref, info in sorted_parts:
                qty = info['quantity']
                comment = info['comment']
                unit_price = info['price']
                total_price = unit_price * qty
                price_display = f"Σ ${total_price:.2f}" if total_price > 0 else "-"
                marker = " [NO PRICE]" if ref in missing_refs else ""
                comment_display = comment if comment else "-"
                print(f"    - {ref:12} | {comment_display:40} | Qty: {qty:2} | {price_display:>9}{marker}")
        else:
            print("  (no extended parts)")

        if promo_ext_refs:
            print(f"  Promotional Extended parts ({len(promo_ext_refs)} total):")
            # Sort by total price descending (price × quantity)
            sorted_parts = sorted(promo_ext_refs.items(), key=lambda x: x[1]['price'] * x[1]['quantity'], reverse=True)
            for ref, info in sorted_parts:
                qty = info['quantity']
                comment = info['comment']
                unit_price = info['price']
                total_price = unit_price * qty
                price_display = f"Σ ${total_price:.2f}" if total_price > 0 else "-"
                comment_display = comment if comment else "-"
                print(f"    - {ref:12} | {comment_display:40} | Qty: {qty:2} | {price_display:>9}")
        else:
            print("  (no promotional extended parts)")

        if basic_refs:
            print(f"  Basic parts ({len(basic_refs)} total):")
            # Sort by total price descending (price × quantity)
            sorted_parts = sorted(basic_refs.items(), key=lambda x: x[1]['price'] * x[1]['quantity'], reverse=True)
            for ref, info in sorted_parts:
                qty = info['quantity']
                comment = info['comment']
                unit_price = info['price']
                total_price = unit_price * qty
                price_display = f"Σ ${total_price:.2f}" if total_price > 0 else "-"
                comment_display = comment if comment else "-"
                print(f"    - {ref:12} | {comment_display:40} | Qty: {qty:2} | {price_display:>9}")
        else:
            print("  (no basic parts)")


if __name__ == '__main__':
    main()
