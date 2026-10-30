import sys
import numpy as np
from PIL import Image

def count_non_black_pixels(path):
    with Image.open(path) as img:
        # Convert to RGB so "black" is consistently (0, 0, 0)
        arr = np.array(img.convert("RGB"))

    # A pixel is non-black if any channel is non-zero
    non_black = np.any(arr != 0, axis=-1)
    return int(non_black.sum()), arr.shape[0] * arr.shape[1]

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} image.tif")
        sys.exit(1)

    count, total = count_non_black_pixels(sys.argv[1])
    print(f"Non-black pixels: {count} of {total} ({count / total:.2%})")