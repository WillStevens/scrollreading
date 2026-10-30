#!/usr/bin/env python3
"""
rotate_zarr.py — rotate a Zarr array 90 degrees in the x,y plane.

Usage:
    rotate_zarr.py <input.zarr> <output.zarr> <cw|ccw> [--axis-order ORDER] [--path ARRAY_PATH]

Example:
    rotate_zarr.py in.zarr out.zarr cw
        Rotates every x,y slice 90 degrees clockwise. Any other axes
        (z, channel, time, ...) are left untouched — only the array's
        x and y dimensions are affected, and they swap sizes.

    rotate_zarr.py in.zarr out.zarr ccw
        Same, but counter-clockwise.

Assumptions
-----------
By default this assumes the array's dimensions end in (..., z, y, x), which
is the common convention for OME-Zarr / bioimaging volumes. The rotation is
always applied to the y and x axes specifically (wherever they fall in the
array), and leaves any other axes (channel, time, z, ...) untouched.

If your array uses a different axis order, pass --axis-order explicitly,
e.g. --axis-order xyz for an array stored as (x, y, z, ...) at the end.
The order string just needs to contain 'x' and 'y' somewhere.

Because a 90 degree rotation swaps the x and y extents, the output array's
shape differs from the input's at those two axes (all other axes are
unchanged).

Memory behaviour
-----------------
The script never loads the whole array into memory. It walks the output
array chunk-by-chunk and, for each chunk, reads only the corresponding
rectangular region from the input (a plain crop — 90 degree rotation maps
rectangles to rectangles), rotates just that block with numpy, and writes
it out. Peak memory is roughly one chunk's worth of data.
"""

import argparse
import itertools
import sys

import numpy as np
import zarr


def resolve_axis(ndim: int, axis_letter: str, axis_order: str) -> int:
    """Map a letter like 'x' to a concrete array axis index, given that
    `axis_order` describes the trailing dimensions of the array (e.g. 'zyx')."""
    axis_letter = axis_letter.lower()
    axis_order = axis_order.lower()

    if axis_letter not in axis_order:
        raise ValueError(
            f"Axis '{axis_letter}' is not part of the given axis order '{axis_order}'."
        )
    if len(axis_order) > ndim:
        raise ValueError(
            f"Axis order '{axis_order}' has {len(axis_order)} letters but the "
            f"array only has {ndim} dimensions."
        )

    # axis_order describes the LAST len(axis_order) dimensions of the array
    offset_from_end = len(axis_order) - axis_order.index(axis_letter)
    axis = ndim - offset_from_end
    return axis


def iter_chunk_ranges(shape, chunks):
    """Yield one (start, end) tuple per dimension for every chunk in the
    array's chunk grid, as an iterator of tuples-of-ranges."""
    per_dim_ranges = []
    for size, chunk in zip(shape, chunks):
        starts = range(0, size, chunk)
        per_dim_ranges.append([(s, min(s + chunk, size)) for s in starts])
    return itertools.product(*per_dim_ranges)


def open_array(path, array_path=None):
    node = zarr.open(path, mode="r")
    if isinstance(node, zarr.Group):
        if array_path:
            node = node[array_path]
        else:
            array_keys = [k for k, v in node.arrays()]
            if len(array_keys) == 1:
                node = node[array_keys[0]]
            else:
                raise ValueError(
                    f"'{path}' is a Zarr group containing multiple arrays "
                    f"{array_keys}. Specify which one with --path."
                )
    if not isinstance(node, zarr.Array):
        raise ValueError(f"Could not resolve '{path}' to a Zarr array.")
    return node


def create_output(output_path, shape, chunks, dtype, fill_value):
    """Create a new zarr array. We deliberately don't try to copy the
    compressor/filter configuration, since that API differs incompatibly
    between zarr-python v2 and v3 — the output just uses that version's
    default compressor instead."""
    return zarr.open(
        output_path,
        mode="w",
        shape=shape,
        chunks=chunks,
        dtype=dtype,
        fill_value=fill_value,
    )


def rotate_zarr(input_path, output_path, direction, axis_order="zyx", array_path=None):
    if direction not in ("cw", "ccw"):
        raise ValueError("direction must be 'cw' or 'ccw'")
    k = -1 if direction == "cw" else 1  # matches np.rot90's k convention

    src = open_array(input_path, array_path=array_path)
    ndim = src.ndim
    axis_y = resolve_axis(ndim, "y", axis_order)
    axis_x = resolve_axis(ndim, "x", axis_order)

    Y = src.shape[axis_y]
    X = src.shape[axis_x]

    out_shape = list(src.shape)
    out_shape[axis_y] = X
    out_shape[axis_x] = Y

    out_chunks = list(src.chunks)
    out_chunks[axis_y] = src.chunks[axis_x]
    out_chunks[axis_x] = src.chunks[axis_y]

    try:
        fill_value = src.fill_value
    except Exception:
        fill_value = 0

    dst = create_output(output_path, tuple(out_shape), tuple(out_chunks), src.dtype, fill_value)

    total_chunks = 1
    for size, chunk in zip(out_shape, out_chunks):
        total_chunks *= -(-size // chunk)  # ceil division

    done = 0
    report_every = max(1, total_chunks // 20)

    for chunk_ranges in iter_chunk_ranges(out_shape, out_chunks):
        out_slices = tuple(slice(start, end) for start, end in chunk_ranges)

        i0, i1 = chunk_ranges[axis_y]  # output range along the y-position axis
        j0, j1 = chunk_ranges[axis_x]  # output range along the x-position axis

        src_slices = list(out_slices)
        if k == 1:  # counter-clockwise
            src_slices[axis_y] = slice(j0, j1)
            src_slices[axis_x] = slice(X - i1, X - i0)
        else:  # clockwise
            src_slices[axis_y] = slice(Y - j1, Y - j0)
            src_slices[axis_x] = slice(i0, i1)
        src_slices = tuple(src_slices)

        block = src[src_slices]
        rotated = np.rot90(block, k=k, axes=(axis_y, axis_x))
        dst[out_slices] = rotated

        done += 1
        if done % report_every == 0 or done == total_chunks:
            pct = 100.0 * done / total_chunks
            print(f"\r  {done}/{total_chunks} chunks ({pct:.0f}%)", end="", file=sys.stderr)

    print(file=sys.stderr)
    return dst


def main():
    parser = argparse.ArgumentParser(
        description="Rotate a Zarr array 90 degrees in the x,y plane.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("input_zarr", help="Path to the input .zarr array or group")
    parser.add_argument("output_zarr", help="Path to write the rotated .zarr array")
    parser.add_argument("direction", choices=["cw", "ccw", "CW", "CCW"],
                         help="Rotate clockwise (cw) or counter-clockwise (ccw)")
    parser.add_argument(
        "--axis-order",
        default="zyx",
        help="Letters describing the array's trailing dimensions, in order "
             "(default: 'zyx', i.e. array is (..., z, y, x)). Must contain "
             "both 'x' and 'y'.",
    )
    parser.add_argument(
        "--path",
        default=None,
        help="If the input is a Zarr group with multiple arrays, the path "
             "of the array to rotate within that group (e.g. '0' for an "
             "OME-Zarr multiscale level).",
    )
    args = parser.parse_args()

    try:
        rotate_zarr(
            args.input_zarr,
            args.output_zarr,
            args.direction.lower(),
            axis_order=args.axis_order,
            array_path=args.path,
        )
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Done. Wrote rotated array to '{args.output_zarr}'.")


if __name__ == "__main__":
    main()