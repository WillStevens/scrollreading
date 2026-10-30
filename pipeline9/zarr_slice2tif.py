#!/usr/bin/env python3
"""
zarr_slice2tif - extract a single Z slice from a Zarr volume and write it as a TIFF.

Usage:
    zarr_slice2tif <zarr> <z-coord> <tif> [--bbox xmin ymin xmax ymax]

The slice is written at the full resolution of the Zarr array (no downsampling).
If the Zarr path is an OME-Zarr / multiscale group, the highest-resolution level
is used by default (override with --level).

Bounding box is given in pixel coordinates as xmin ymin xmax ymax, with the max
values exclusive (numpy convention).  Use --inclusive to treat them as inclusive.

Examples:
    zarr_slice2tif volume.zarr 512 slice512.tif
    zarr_slice2tif volume.zarr 512 crop.tif --bbox 1000 2000 3000 4000
    zarr_slice2tif volume.zarr 512 crop.tif -b 1000 2000 3000 4000 --compression zlib
"""

import argparse
import os
import sys

import numpy as np
import tifffile
import zarr


# --------------------------------------------------------------------------- #
# Opening the zarr
# --------------------------------------------------------------------------- #

def _is_array(node):
    return hasattr(node, "shape") and hasattr(node, "dtype") and hasattr(node, "__getitem__")


def _iter_arrays(group, prefix=""):
    """Yield (path, array) for every array in a group, recursing one level deep."""
    try:
        items = list(group.arrays())
    except AttributeError:
        items = [(k, group[k]) for k in group.array_keys()]
    for name, arr in items:
        yield (prefix + name, arr)

    try:
        subgroups = list(group.groups())
    except AttributeError:
        subgroups = [(k, group[k]) for k in getattr(group, "group_keys", lambda: [])()]
    for name, sub in subgroups:
        yield from _iter_arrays(sub, prefix + name + "/")


def open_array(path, level=None):
    """Open a zarr path and return the array to sample from."""
    node = zarr.open(path, mode="r")

    if _is_array(node):
        if level not in (None, 0):
            sys.exit(f"error: {path} is a plain array; --level {level} is not valid")
        return node, ""

    # It's a group. Prefer OME-Zarr multiscales metadata.
    attrs = dict(node.attrs)
    multiscales = attrs.get("multiscales")
    if multiscales:
        datasets = multiscales[0].get("datasets", [])
        paths = [d["path"] for d in datasets]
        if paths:
            idx = 0 if level is None else level
            if idx < 0 or idx >= len(paths):
                sys.exit(f"error: --level {idx} out of range (0..{len(paths) - 1})")
            return node[paths[idx]], paths[idx]

    # No multiscale metadata: collect arrays and pick one.
    found = list(_iter_arrays(node))
    if not found:
        sys.exit(f"error: no arrays found in {path}")

    if level is not None:
        # Try to interpret level as a named subarray ("0", "1", ...).
        by_name = dict(found)
        key = str(level)
        if key in by_name:
            return by_name[key], key
        sys.exit(f"error: no array named '{key}' in {path}; "
                 f"available: {', '.join(n for n, _ in found)}")

    if len(found) == 1:
        return found[0][1], found[0][0]

    # Multiple arrays, no metadata: choose the largest (highest resolution).
    name, arr = max(found, key=lambda t: int(np.prod(t[1].shape)))
    print(f"note: group contains {len(found)} arrays; using largest: '{name}' {arr.shape}",
          file=sys.stderr)
    return arr, name


# --------------------------------------------------------------------------- #
# Slicing
# --------------------------------------------------------------------------- #

def build_index(arr, z, bbox, leading):
    """Build a tuple of slices/ints selecting the requested region.

    Assumes the last three axes are (z, y, x).  Axes before those are indexed
    with the values given by --leading (default 0 for each).
    """
    ndim = arr.ndim
    if ndim < 3:
        sys.exit(f"error: array has {ndim} dimensions; need at least 3 (z, y, x)")

    nlead = ndim - 3
    if leading is None:
        leads = [0] * nlead
    else:
        if len(leading) != nlead:
            sys.exit(f"error: array has {nlead} leading axis/axes before (z, y, x); "
                     f"--leading needs exactly {nlead} value(s)")
        leads = list(leading)

    for i, v in enumerate(leads):
        n = arr.shape[i]
        if not (-n <= v < n):
            sys.exit(f"error: leading index {v} out of range for axis {i} (size {n})")

    nz, ny, nx = arr.shape[-3:]
    if not (-nz <= z < nz):
        sys.exit(f"error: z={z} out of range (array has {nz} slices, 0..{nz - 1})")

    if bbox is None:
        ys, xs = slice(0, ny), slice(0, nx)
    else:
        xmin, ymin, xmax, ymax = bbox
        xmin = max(0, min(xmin, nx))
        ymin = max(0, min(ymin, ny))
        xmax = max(0, min(xmax, nx))
        ymax = max(0, min(ymax, ny))
        if xmax <= xmin or ymax <= ymin:
            sys.exit("error: bounding box is empty after clipping to the array "
                     f"(image is {nx} x {ny} pixels)")
        ys, xs = slice(ymin, ymax), slice(xmin, xmax)

    return tuple(leads) + (z, ys, xs)


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #

def parse_args(argv=None):
    p = argparse.ArgumentParser(
        prog="zarr_slice2tif",
        description="Write one Z slice of a Zarr volume to a TIFF at full resolution.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Bounding box is xmin ymin xmax ymax in pixels; max values are "
               "exclusive unless --inclusive is given.",
    )
    p.add_argument("zarr", help="path (or URL) of the zarr array or group")
    p.add_argument("z", type=int, help="z coordinate of the slice")
    p.add_argument("tif", help="output TIFF filename")
    p.add_argument("-b", "--bbox", type=int, nargs=4,
                   metavar=("XMIN", "YMIN", "XMAX", "YMAX"),
                   help="region to convert (default: whole slice)")
    p.add_argument("--inclusive", action="store_true",
                   help="treat XMAX/YMAX as inclusive")
    p.add_argument("--level", type=int, default=None,
                   help="multiscale level to read (default: 0, the full resolution)")
    p.add_argument("--leading", type=int, nargs="+", default=None, metavar="I",
                   help="indices for any axes preceding (z, y, x), e.g. time/channel")
    p.add_argument("--compression", default="none",
                   choices=["none", "zlib", "lzw", "lzma", "zstd", "packbits"],
                   help="TIFF compression (default: none)")
    p.add_argument("--bigtiff", action="store_true",
                   help="force BigTIFF output (automatic for images over ~3.9 GB)")
    p.add_argument("-q", "--quiet", action="store_true", help="suppress progress output")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)

    arr, name = open_array(args.zarr, args.level)

    bbox = args.bbox
    if bbox is not None and args.inclusive:
        bbox = [bbox[0], bbox[1], bbox[2] + 1, bbox[3] + 1]

    index = build_index(arr, args.z, bbox, args.leading)

    if not args.quiet:
        label = f"{args.zarr}" + (f"[{name}]" if name else "")
        print(f"reading {label} shape={arr.shape} dtype={arr.dtype}", file=sys.stderr)

    data = np.asarray(arr[index])

    # A trailing channel axis (e.g. RGB) survives the slice; anything else is odd.
    if data.ndim == 2:
        photometric = "minisblack"
    elif data.ndim == 3 and data.shape[-1] in (3, 4):
        photometric = "rgb"
    else:
        photometric = "minisblack"

    nbytes = data.nbytes
    bigtiff = args.bigtiff or nbytes > 3_900_000_000
    compression = None if args.compression == "none" else args.compression

    outdir = os.path.dirname(os.path.abspath(args.tif))
    if outdir:
        os.makedirs(outdir, exist_ok=True)

    tifffile.imwrite(
        args.tif,
        data,
        photometric=photometric,
        compression=compression,
        bigtiff=bigtiff,
        metadata={"axes": "YXS" if photometric == "rgb" else "YX"},
    )

    if not args.quiet:
        h, w = data.shape[:2]
        print(f"wrote {args.tif}: {w} x {h} px, {data.dtype}, "
              f"{nbytes / 1e6:.1f} MB uncompressed", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())