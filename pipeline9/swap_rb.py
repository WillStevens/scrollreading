import sys
from pathlib import Path
from PIL import Image

def swap_red_blue(folder):
    for path in Path(folder).glob("*.png"):
        if path.name.startswith("b_"):
            continue  # skip files we've already produced
        img = Image.open(path)
        has_alpha = img.mode in ("RGBA", "LA") or "transparency" in img.info
        img = img.convert("RGBA" if has_alpha else "RGB")

        if has_alpha:
            r, g, b, a = img.split()
            out = Image.merge("RGBA", (b, g, r, a))
        else:
            r, g, b = img.split()
            out = Image.merge("RGB", (b, g, r))

        out.save(path.with_name("b_" + path.name))
        print(f"Wrote b_{path.name}")

if __name__ == "__main__":
    folder = sys.argv[1] if len(sys.argv) > 1 else "."
    swap_red_blue(folder)