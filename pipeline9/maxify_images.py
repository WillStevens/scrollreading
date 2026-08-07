#!/usr/bin/env python3
"""
pad_tifs.py

Takes a folder of .tif/.tiff images, finds the largest width and largest
height among them, then creates a new folder where every image is placed
on a black canvas of that (max_width x max_height) size, centered.

Usage:
    python pad_tifs.py /path/to/input_folder /path/to/output_folder

If output folder is omitted, it defaults to "<input_folder>_padded".
"""

import os
import sys
from pathlib import Path
from PIL import Image


def find_max_dimensions(input_dir: Path):
    """Scan all tif/tiff files and return (max_width, max_height)."""
    max_w, max_h = 0, 0
    tif_files = []

    for f in sorted(input_dir.iterdir()):
        if f.suffix.lower() in (".tif", ".tiff"):
            try:
                with Image.open(f) as img:
                    w, h = img.size
                    max_w = max(max_w, w)
                    max_h = max(max_h, h)
                    tif_files.append(f)
            except Exception as e:
                print(f"  Warning: could not read {f.name}: {e}")

    return max_w, max_h, tif_files


def pad_image(img: Image.Image, target_w: int, target_h: int) -> Image.Image:
    """Place img centered on a black canvas of size (target_w, target_h)."""
    # Preserve the image's mode (e.g. RGB, RGBA, L, I;16) so colors/black
    # levels behave correctly for that mode.
    mode = img.mode

    # Black fill value depends on mode: 0 for most, (0,0,0[,0]) for multi-band
    if mode in ("RGB",):
        fill = (0, 0, 0)
    elif mode in ("RGBA",):
        fill = (0, 0, 0, 255)  # opaque black
    else:
        fill = 0

    canvas = Image.new(mode, (target_w, target_h), fill)

    w, h = img.size
    offset_x = (target_w - w) // 2
    offset_y = (target_h - h) // 2

    canvas.paste(img, (offset_x, offset_y))
    return canvas


def main():
    if len(sys.argv) < 2:
        print("Usage: python pad_tifs.py <input_folder> [output_folder]")
        sys.exit(1)

    input_dir = Path(sys.argv[1]).expanduser().resolve()
    if not input_dir.is_dir():
        print(f"Error: {input_dir} is not a valid directory")
        sys.exit(1)

    if len(sys.argv) >= 3:
        output_dir = Path(sys.argv[2]).expanduser().resolve()
    else:
        output_dir = input_dir.parent / f"{input_dir.name}_padded"

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Scanning {input_dir} for TIF files...")
    max_w, max_h, tif_files = find_max_dimensions(input_dir)

    if not tif_files:
        print("No .tif/.tiff files found. Exiting.")
        sys.exit(0)

    print(f"Found {len(tif_files)} TIF file(s).")
    print(f"Target canvas size: {max_w} x {max_h}")
    print(f"Output folder: {output_dir}\n")

    for f in tif_files:
        try:
            with Image.open(f) as img:
                img.load()  # ensure fully loaded before we close the file
                padded = pad_image(img, max_w, max_h)
                out_path = output_dir / f.name
                padded.save(out_path)
                print(f"  {f.name}: {img.size} -> {padded.size}  saved")
        except Exception as e:
            print(f"  Error processing {f.name}: {e}")

    print("\nDone.")


if __name__ == "__main__":
    main()