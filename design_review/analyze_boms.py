#!/usr/bin/env python3
"""Analyze BOMs to compute total price and count of distinct extended parts."""

import csv
import sys
from pathlib import Path


def analyze_bom(filepath):
    """
    Analyze a single BOM file.
    Returns: (total_price, num_extended_parts, num_missing_price)
    """
    total_price = 0.0
    extended_parts = set()
    missing_price_parts = set()

    # BOMs are UTF-16 encoded
    with open(filepath, 'r', encoding='utf-16') as f:
        reader = csv.DictReader(f, delimiter='\t')

        for row in reader:
            # Get quantity and price
            try:
                quantity = int(row.get('Quantity', '0'))
                price_str = row.get('JLCPCB Price', '0')
                price = float(price_str) if price_str else 0.0
                supplier_part = row.get('Supplier Part', '').strip()

                # Accumulate total price
                total_price += quantity * price

                # Track extended parts and those with missing price
                part_class = row.get('JLCPCB Part Class', '')
                if part_class.strip() == 'Extended Part':
                    if supplier_part:
                        extended_parts.add(supplier_part)

                    # Track missing price (price is 0 or empty)
                    if price == 0.0 and supplier_part:
                        missing_price_parts.add(supplier_part)

            except (ValueError, KeyError) as e:
                print(f"Warning: Error parsing row in {filepath.name}: {e}", file=sys.stderr)
                continue

    return total_price, len(extended_parts), len(missing_price_parts), extended_parts, missing_price_parts


def main():
    """Process all BOM files in the export directory."""
    export_dir = Path(__file__).parent / 'export'

    if not export_dir.exists():
        print(f"Error: export directory not found at {export_dir}")
        sys.exit(1)

    # Find all BOM CSV files
    bom_files = sorted(export_dir.glob('BOM_*.csv'))

    if not bom_files:
        print(f"No BOM files found in {export_dir}")
        sys.exit(1)

    print(f"{'BOM Name':<40} {'Total Price':>15} {'Extended':>12} {'No Price':>12}")
    print("-" * 80)

    total_all = 0.0
    total_extended_all = 0
    total_missing_all = 0
    all_extended_parts = set()
    all_missing_parts = set()

    results = []
    for bom_file in bom_files:
        price, num_extended, num_missing, extended_refs, missing_refs = analyze_bom(bom_file)
        results.append((bom_file.stem, price, num_extended, num_missing, extended_refs, missing_refs))
        total_all += price
        total_extended_all += num_extended
        total_missing_all += num_missing
        all_extended_parts.update(extended_refs)
        all_missing_parts.update(missing_refs)

        print(f"{bom_file.stem:<40} ${price:>14.2f} {num_extended:>12} {num_missing:>12}")

    print("-" * 80)
    print(f"{'TOTAL':<40} ${total_all:>14.2f} {total_extended_all:>12} {total_missing_all:>12}")
    print(f"{'DISTINCT ACROSS BOARDS':<40} {'':<15} {len(all_extended_parts):>12} {len(all_missing_parts):>12}")
    print()

    # Detailed extended parts report
    print("\nDetailed Extended Parts Report:")
    print("=" * 75)

    for bom_name, price, num_extended, num_missing, extended_refs, missing_refs in results:
        print(f"\n{bom_name}:")
        if extended_refs:
            print(f"  Extended parts ({len(extended_refs)} total):")
            for ref in sorted(extended_refs):
                marker = " [NO PRICE]" if ref in missing_refs else ""
                print(f"    - {ref}{marker}")
        else:
            print("  (no extended parts)")


if __name__ == '__main__':
    main()
