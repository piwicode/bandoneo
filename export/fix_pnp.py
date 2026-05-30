#!/usr/bin/env python3
"""Fix EasyEDA pick-and-place centroid bug.

EasyEDA computes incorrect Mid X/Y for components with non-90-degree rotations.
This script overwrites Mid X/Y with Ref X/Y for those components and verifies
that components at multiples of 90° already have correct Mid X/Y.
"""

import csv
import codecs
import glob
import sys
from pathlib import Path
from collections import defaultdict

EPSILON = 1e-4  # mm tolerance for float comparison

# Through-hole / hand-placed parts absent from the SMD BOM — skip centroid check.
NOT_IN_BOM = {"STDC14_Connector"}


def parse_mm(val: str) -> float:
    return float(val.strip().removesuffix("mm"))


def is_multiple_of_90(rotation: float) -> bool:
    return abs(rotation % 90) < EPSILON or abs(rotation % 90 - 90) < EPSILON


def process_file(path: Path) -> bool:
    """Process one pick-and-place file. Returns True on success."""
    ok = True

    with codecs.open(path, "r", encoding="utf-16") as f:
        reader = csv.reader(f, delimiter="\t")
        header = next(reader)
        rows = list(reader)

    # Column indices (0-based)
    # Designator Device Footprint Ref X Ref Y Pad X Pad Y Pins Layer Rotation SMD Comment Mid Y Mid X
    IDX = {name: i for i, name in enumerate(header)}
    try:
        I_DESIGNATOR = IDX["Designator"]
        I_DEVICE     = IDX["Device"]
        I_REF_X      = IDX["Ref X"]
        I_REF_Y      = IDX["Ref Y"]
        I_ROTATION   = IDX["Rotation"]
        I_MID_X      = IDX["Mid X"]
        I_MID_Y      = IDX["Mid Y"]
    except KeyError as e:
        print(f"ERROR: {path.name}: missing column {e}")
        return False

    errors = []       # (designator, device, field, expected, got)
    fixed_rows = []   # (designator, device, rotation)
    out_rows = []

    for row in rows:
        if not row or len(row) <= max(I_ROTATION, I_MID_X, I_MID_Y):
            out_rows.append(row)
            continue

        try:
            rotation = parse_mm(row[I_ROTATION])  # rotation has no unit but parse_mm handles plain floats
        except ValueError:
            out_rows.append(row)
            continue

        ref_x_str = row[I_REF_X]
        ref_y_str = row[I_REF_Y]
        mid_x_str = row[I_MID_X]
        mid_y_str = row[I_MID_Y]

        try:
            ref_x = parse_mm(ref_x_str)
            ref_y = parse_mm(ref_y_str)
            mid_x = parse_mm(mid_x_str)
            mid_y = parse_mm(mid_y_str)
        except ValueError:
            out_rows.append(row)
            continue

        row = list(row)

        if is_multiple_of_90(rotation):
            if row[I_DEVICE] in NOT_IN_BOM:
                pass
            elif abs(mid_x - ref_x) > EPSILON or abs(mid_y - ref_y) > EPSILON:
                errors.append((
                    row[I_DESIGNATOR], row[I_DEVICE], rotation,
                    f"({ref_x_str},{ref_y_str})", f"({mid_x_str},{mid_y_str})"
                ))
        else:
            if abs(mid_x - ref_x) > EPSILON or abs(mid_y - ref_y) > EPSILON:
                fixed_rows.append((row[I_DESIGNATOR], row[I_DEVICE], rotation))
                row[I_MID_X] = ref_x_str
                row[I_MID_Y] = ref_y_str

        out_rows.append(row)

    # Report errors (fail hard)
    if errors:
        print(f"\nERROR: {path.name}: Mid X/Y != Ref X/Y for 90°-multiple rotations:")
        for des, dev, rot, ref, mid in errors:
            print(f"  {des} ({dev}) rot={rot:.0f}°  ref={ref}  mid={mid}")
        ok = False

    # Report fixes grouped by device
    if fixed_rows:
        by_device: dict[str, list] = defaultdict(list)
        for des, dev, rot in fixed_rows:
            by_device[dev].append((des, rot))

        print(f"\n{path.name}: fixed {len(fixed_rows)} component(s):")
        for dev in sorted(by_device):
            comps = sorted(by_device[dev], key=lambda t: t[0])
            desigs = ", ".join(f"{d}({r:.0f}°)" for d, r in comps)
            print(f"  [{dev}]  {desigs}")
    else:
        print(f"\n{path.name}: no non-90° components to fix")

    if not ok:
        return False

    # Write output file — preserve original style: header unquoted, data values quoted.
    def fmt_row(fields: list[str]) -> str:
        parts = [f'"{v}"' if v else "" for v in fields]
        return "\t".join(parts) + "\r\n"

    out_path = path.with_stem(path.stem + "_fixed")
    with codecs.open(out_path, "w", encoding="utf-16") as f:
        f.write("\t".join(header) + "\r\n")
        for row in out_rows:
            f.write(fmt_row(row))

    print(f"  → wrote {out_path.name}")
    return True


def main():
    pattern = Path(__file__).parent / "PickAndPlace_*.csv"
    files = sorted(
        p for p in glob.glob(str(pattern))
        if not p.endswith("_fixed.csv")
    )

    if not files:
        print("No PickAndPlace_*.csv files found.")
        sys.exit(1)

    all_ok = True
    for path in files:
        if not process_file(Path(path)):
            all_ok = False

    if not all_ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
