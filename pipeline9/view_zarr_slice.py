#!/usr/bin/env python3
"""
view_zarr_slice.py
Open a Zarr volume and display a 2D slice at a specified Z coordinate.

Usage:
    python view_zarr_slice.py <zarr_path> <z>
    python view_zarr_slice.py <zarr_path> <z> --level 0
    python view_zarr_slice.py scroll1.zarr 500
"""

import argparse
import sys

import zarr
import matplotlib.pyplot as plt
import numpy as np


def open_zarr(path: str):
    """Open a Zarr store (directory, zip, or remote URL)."""
    try:
        #store = zarr.storage.LocalStore(path)
        #z = zarr.open_group(store=store,mode="r")
        z = zarr.open(path, mode="r")
        return z
    except Exception as e:
        print(f"Error opening Zarr at '{path}': {e}", file=sys.stderr)
        sys.exit(1)


def get_array(store, level: int = 0):
    """
    Retrieve the array from the store.
    Handles:
      - Plain array (zarr.Array)
      - Group with numeric keys  (OME-Zarr multiscale: '0', '1', ...)
      - Group with a single array child
    """
    if isinstance(store, zarr.Array):
        return store

    if isinstance(store, zarr.Group):
        # OME-Zarr multiscale layout: levels stored as '0', '1', ...
        key = str(level)
        if key in store:
            arr = store[key]
            if isinstance(arr, zarr.Array):
                return arr

        # Fall back: return the first array child found
        for name, item in store.items():
            if isinstance(item, zarr.Array):
                print(f"Using array '{name}' from group.")
                return item

    print(
        "Could not locate a usable array in the Zarr store. "
        "Try specifying a sub-path, e.g. 'mystore.zarr/0'.",
        file=sys.stderr,
    )
    sys.exit(1)


def display_slice(arr: zarr.Array, z: int, output: str | None = None):
    """Load and display the XY slice at index z (first axis)."""
    ndim = arr.ndim
    shape = arr.shape

    print(f"Array shape : {shape}")
    print(f"Array dtype : {arr.dtype}")
    print(f"Chunks      : {arr.chunks}")

    # Support shapes like (Z, Y, X) or (Z, Y, X) with a leading channel dim
    if ndim == 3:
        z_size = shape[0]
    elif ndim == 4:
        # e.g. (C, Z, Y, X) — take channel 0
        z_size = shape[1]
        print("4-D array detected; using channel 0.")
    else:
        print(f"Unsupported number of dimensions: {ndim}", file=sys.stderr)
        sys.exit(1)

    if not (0 <= z < z_size):
        print(
            f"Z index {z} is out of range — array has {z_size} slices (0–{z_size - 1}).",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"Loading slice z={z} …")
    if ndim == 3:
        img = arr[z, :, :]
    else:
        img = arr[0, z, :, :]

    img = np.array(img)  # materialise from lazy store

    # --- Plot ---
    fig, ax = plt.subplots(figsize=(8, 8))
    ax.imshow(img, cmap="gray", interpolation="nearest")
    ax.set_title(f"Z slice {z}  —  shape {img.shape}  dtype {img.dtype}")
    ax.axis("off")
    plt.tight_layout()

    if output:
        fig.savefig(output, dpi=150)
        print(f"Saved to {output}")
    else:
        plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Display a 2-D slice from a Zarr volume at a given Z index."
    )
    parser.add_argument("zarr_path", help="Path (or URL) to the Zarr store")
    parser.add_argument("z", type=int, help="Z index of the slice to display")
    parser.add_argument(
        "--level",
        type=int,
        default=0,
        help="Resolution level for multiscale Zarr (default: 0 = full resolution)",
    )
    parser.add_argument(
        "--output",
        "-o",
        default=None,
        help="Save the slice to this file instead of showing it (e.g. slice.png)",
    )
    args = parser.parse_args()

    store = open_zarr(args.zarr_path)
    arr = get_array(store, level=args.level)
    display_slice(arr, args.z, output=args.output)


if __name__ == "__main__":
    main()
