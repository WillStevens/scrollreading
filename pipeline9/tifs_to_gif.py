#!/usr/bin/env python3
"""
tifs_to_gif.py

Takes a folder of .tif/.tiff images and combines them into a single
animated GIF, using the sorted filename order as the frame order.

Usage:
    python tifs_to_gif.py <input_folder> [output.gif] [--duration MS] [--loop N]

Arguments:
    input_folder   Folder containing .tif/.tiff files (one frame per file)
    output.gif     Optional path for the output GIF (default: <folder>.gif
                    next to the input folder)
    --duration MS  Duration of each frame in milliseconds (default: 200)
    --loop N       Number of times to loop (default: 0 = loop forever)

Notes:
    - Frames are sorted alphabetically by filename, so name them like
      frame001.tif, frame002.tif, ... if order matters.
    - All frames are resized to match the first frame's dimensions if
      they differ (GIFs need consistent frame sizes).
    - Images are converted to RGB, since GIF output doesn't support
      arbitrary source modes (Pillow will palette-quantize automatically).
"""

import sys
import argparse
from pathlib import Path
from PIL import Image


def load_tif_frames(input_dir: Path):
    """Load all tif/tiff files from a folder, sorted by filename."""
    files = sorted(
        f for f in input_dir.iterdir()
        if f.suffix.lower() in (".tif", ".tiff")
    )

    if not files:
        return [], []

    frames = []
    for f in files:
        try:
            img = Image.open(f)
            img.load()
            frames.append(img)
        except Exception as e:
            print(f"  Warning: could not read {f.name}: {e}")

    return frames, files


def normalize_frames(frames):
    """Ensure all frames share the same size and are in RGB mode."""
    base_size = frames[0].size
    normalized = []

    for img in frames:
        if img.mode != "RGB":
            img = img.convert("RGB")
        if img.size != base_size:
            img = img.resize(base_size, Image.LANCZOS)
        normalized.append(img)

    return normalized


def main():
    parser = argparse.ArgumentParser(description="Convert a folder of TIFs into an animated GIF.")
    parser.add_argument("input_folder", help="Folder containing .tif/.tiff files")
    parser.add_argument("output_gif", nargs="?", default=None, help="Output .gif path")
    parser.add_argument("--duration", type=int, default=200, help="Frame duration in ms (default: 200)")
    parser.add_argument("--loop", type=int, default=0, help="Loop count, 0 = infinite (default: 0)")
    args = parser.parse_args()

    input_dir = Path(args.input_folder).expanduser().resolve()
    if not input_dir.is_dir():
        print(f"Error: {input_dir} is not a valid directory")
        sys.exit(1)

    if args.output_gif:
        output_path = Path(args.output_gif).expanduser().resolve()
    else:
        output_path = input_dir.parent / f"{input_dir.name}.gif"

    print(f"Scanning {input_dir} for TIF files...")
    frames, files = load_tif_frames(input_dir)

    if not frames:
        print("No .tif/.tiff files found. Exiting.")
        sys.exit(0)

    print(f"Found {len(frames)} frame(s):")
    for f in files:
        print(f"  {f.name}")

    print("Normalizing frame sizes/mode...")
    frames = normalize_frames(frames)

    print(f"Saving animated GIF to {output_path} "
          f"(duration={args.duration}ms/frame, loop={args.loop})...")

    frames[0].save(
        output_path,
        format="GIF",
        save_all=True,
        append_images=frames[1:],
        duration=args.duration,
        loop=args.loop,
        optimize=False,
    )

    print("Done.")


if __name__ == "__main__":
    main()