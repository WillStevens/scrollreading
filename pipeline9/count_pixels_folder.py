import csv
import sys
from pathlib import Path

import numpy as np
from PIL import Image

EXTENSIONS = {".tif", ".tiff", ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}


def count_non_black_pixels(path):
    with Image.open(path) as img:
        # Normalise modes where "black" would otherwise be ambiguous,
        # but leave 16-bit / grayscale modes alone to avoid losing precision
        if img.mode in ("RGBA", "RGBa", "P", "PA", "CMYK", "YCbCr", "LAB", "HSV"):
            img = img.convert("RGB")
        elif img.mode == "LA":
            img = img.convert("L")
        arr = np.array(img)

    if arr.ndim == 3:
        non_black = np.any(arr != 0, axis=-1)  # non-black if any channel is non-zero
    else:
        non_black = arr != 0

    return int(non_black.sum()), non_black.size


def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <folder> [output.csv]", file=sys.stderr)
        sys.exit(1)

    folder = Path(sys.argv[1])
    if not folder.is_dir():
        print(f"Not a folder: {folder}", file=sys.stderr)
        sys.exit(1)

    out = open(sys.argv[2], "w", newline="") if len(sys.argv) > 2 else sys.stdout
    writer = csv.writer(out)
    writer.writerow(["filename", "non_black_pixels", "total_pixels"])

    total_non_black = 0
    total_pixels = 0

    files = sorted(p for p in folder.iterdir() if p.suffix.lower() in EXTENSIONS)
    for path in files:
        try:
            count, size = count_non_black_pixels(path)
        except Exception as e:
            print(f"Skipping {path.name}: {e}", file=sys.stderr)
            continue
        writer.writerow([path.name, count, size])
        total_non_black += count
        total_pixels += size

    writer.writerow(["TOTAL", total_non_black, total_pixels])

    if out is not sys.stdout:
        out.close()
    print(f"Processed {len(files)} files. Total non-black pixels: {total_non_black}",
          file=sys.stderr)


if __name__ == "__main__":
    main()