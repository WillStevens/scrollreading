#!/usr/bin/env python3
"""
crop_images.py

For each pair of files s_nnnnnnnn.csv / s_nnnnnnnn.tif in a folder (where
nnnnnnnn is a zero-padded 8-digit number), read xmin,ymin,xmax,ymax from the
csv and crop the corresponding tif to that box, saving the result as
c_nnnnnnnn.tif in the same folder.

CSV format (no header assumed): z,xmin,ymin,xmax,ymax,<patches...>
Only the first data row of each csv is used.

Usage:
    python crop_images.py /path/to/folder 0 99
    python crop_images.py /path/to/folder --lower 0 --upper 99

This will process s_00000000.csv/.tif through s_00000099.csv/.tif.
"""

import argparse
import csv
import sys
from pathlib import Path

from PIL import Image


def read_box(csv_path: Path):
    """Read the first data row of the csv and return (xmin, ymin, xmax, ymax) as ints."""
    with open(csv_path, newline="") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            row = [c.strip() for c in row]
            # Skip a header row if present (non-numeric first field)
            try:
                float(row[0])
            except ValueError:
                continue
            if len(row) < 5:
                raise ValueError(
                    f"{csv_path.name}: expected at least 5 columns (z,xmin,ymin,xmax,ymax), "
                    f"got {len(row)}"
                )
            _z, xmin, ymin, xmax, ymax = row[0], row[1], row[2], row[3], row[4]
            return (
                int(float(xmin)),
                int(float(ymin)),
                int(float(xmax)),
                int(float(ymax)),
            )
    raise ValueError(f"{csv_path.name}: no data rows found")


def process_one(folder: Path, n: int, m: int, overwrite: bool) -> bool:
    """Process a single index n. Returns True on success, False if skipped/missing."""
    stem = f"{n:08d}"
    out_stem = f"{m:08d}"
    csv_path = folder / f"s_{stem}.csv"
    tif_path = folder / f"s_{stem}.tif"
    out_path = folder / f"c_{out_stem}.tif"

    if not csv_path.exists() or not tif_path.exists():
        print(f"[skip] {stem}: missing s_{stem}.csv or s_{stem}.tif")
        return False

    if out_path.exists() and not overwrite:
        print(f"[skip] {stem}: c_{stem}.tif already exists (use --overwrite to replace)")
        return False

    try:
        xmin, ymin, xmax, ymax = read_box(csv_path)
    except ValueError as e:
        print(f"[error] {e}")
        return False

    try:
        with Image.open(tif_path) as img:
            img.load()
            width, height = img.size

            # Clip the box to the image bounds so we don't error on out-of-range values.
            cx_min = max(0, min(xmin, width))
            cy_min = max(0, min(ymin, height))
            cx_max = max(0, min(xmax, width))
            cy_max = max(0, min(ymax, height))

            if cx_max <= cx_min or cy_max <= cy_min:
                print(
                    f"[error] {stem}: invalid/empty crop box after clipping to image "
                    f"({width}x{height}): ({xmin},{ymin},{xmax},{ymax}) -> "
                    f"({cx_min},{cy_min},{cx_max},{cy_max})"
                )
                return False

            if (cx_min, cy_min, cx_max, cy_max) != (xmin, ymin, xmax, ymax):
                print(
                    f"[warn] {stem}: box ({xmin},{ymin},{xmax},{ymax}) clipped to "
                    f"({cx_min},{cy_min},{cx_max},{cy_max}) for image size ({width}x{height})"
                )

            cropped = img.crop((cx_min, cy_min, cx_max, cy_max))
            cropped.save(out_path)
    except Exception as e:
        print(f"[error] {stem}: failed to crop/save image: {e}")
        return False

    print(f"[ok] {stem}: wrote {out_path.name} ({cx_max - cx_min}x{cy_max - cy_min})")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Crop s_nnnnnnnn.tif images using boxes from s_nnnnnnnn.csv, "
        "writing c_nnnnnnnn.tif files."
    )
    parser.add_argument("path", type=str, help="Folder containing the s_*.csv / s_*.tif files")
    parser.add_argument("lower", type=int, help="Lower bound of the nnnnnnnn number (inclusive)")
    parser.add_argument("upper", type=int, help="Upper bound of the nnnnnnnn number (inclusive)")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite c_nnnnnnnn.tif files that already exist (default: skip them)",
    )

    args = parser.parse_args()

    folder = Path(args.path)
    if not folder.is_dir():
        print(f"Error: {folder} is not a directory", file=sys.stderr)
        sys.exit(1)

    if args.lower > args.upper:
        print("Error: lower bound must be <= upper bound", file=sys.stderr)
        sys.exit(1)

    total = 0
    ok = 0
    for n in range(args.lower, args.upper + 1):
        total += 1
        if process_one(folder, n, total, args.overwrite):
            ok += 1

    print(f"\nDone: {ok}/{total} images processed successfully.")


if __name__ == "__main__":
    main()