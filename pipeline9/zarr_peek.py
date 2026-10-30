#!/usr/bin/env python3
"""Print a 10x10x10 block of a 3D zarr array as ten 10x10 ASCII grids.

    python zarr_peek.py mask.zarr 0 0 0
    python zarr_peek.py mask.zarr 100 50 20 --chars
    python zarr_peek.py group.zarr 0 0 0 --array myarray
"""
import argparse
import sys

import zarr

ap = argparse.ArgumentParser()
ap.add_argument("path")
ap.add_argument("i", type=int, help="start index along array axis 0")
ap.add_argument("j", type=int, help="start index along array axis 1")
ap.add_argument("k", type=int, help="start index along array axis 2")
ap.add_argument("--array", help="array name, if path is a group")
ap.add_argument("--chars", action="store_true", help="show '#' for nonzero, '.' for zero")
ap.add_argument("--size", type=int, default=10, help="block edge length (default 10)")
a = ap.parse_args()

print(f"zarr {zarr.__version__}")
arr = zarr.open(a.path, mode="r")
if a.array:
    arr = arr[a.array]
print(f"shape {arr.shape}, chunks {arr.chunks}, dtype {arr.dtype}")

n = a.size
block = arr[a.i:a.i + n, a.j:a.j + n, a.k:a.k + n]   # plain slice, no steps
print(f"block [{a.i}:{a.i+n}, {a.j}:{a.j+n}, {a.k}:{a.k+n}] -> shape {block.shape}\n")

for s in range(block.shape[0]):
    print(f"--- axis0 = {a.i + s} (rows = axis1, cols = axis2) ---")
    for row in block[s]:
        if a.chars:
            print(" ".join("#" if v else "." for v in row))
        else:
            print(" ".join(f"{int(v):3d}" for v in row))
    print()