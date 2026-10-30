#!/usr/bin/env python3
"""
flip_zarr.py — mirror a Zarr array along one spatial axis.

Usage:
    flip_zarr.py <input.zarr> <output.zarr> <x|y|z> [--axis-order ORDER] [--path ARRAY_PATH]

Example:
    flip_zarr.py in.zarr out.zarr z
        Mirrors the volume through the plane that bisects it along z
        (i.e. reverses the z axis — the same effect as reflecting through
        an x,y plane at the mid z coordinate).

Assumptions
-----------
By default this assumes the array's dimensions end in (..., z, y, x), which
is the common convention for OME-Zarr / bioimaging volumes (a 3D array is
(z, y, x); a 4D array might be (c, z, y, x); a 5D array (t, c, z, y, x)).
Any leading axes (channel, time, etc.) are left untouched.

If your array uses a different axis order, pass --axis-order explicitly,
e.g. --axis-order xyz for an array stored as (x, y, z, ...) at the end,
or --axis-order zyx (the default).

Memory behaviour
-----------------
The script never loads the whole array into memory. It walks the output
array chunk-by-chunk (using the array's own chunk grid) and, for each
chunk, reads only the mirrored source region, flips it, and writes it out.
Peak memory is roughly one chunk's worth of data.
"""

import argparse
import itertools
import sys

import numpy as np
import zarr


def resolve_flip_axis(ndim: int, axis_letter: str, axis_order: str) -> int:
    """Map a letter like 'z' to a concrete array axis index, given that
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
            # try to find a single array in the group automatically
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


def create_like(output_path, src):
    """Create a new zarr array at output_path with the same shape/chunks/
    dtype as src. We deliberately don't try to copy the compressor/filter
    configuration, since that API differs incompatibly between zarr-python
    v2 and v3 — the output will just use that zarr version's default
    compressor instead."""
    try:
        fill_value = src.fill_value
    except Exception:
        fill_value = 0
    return zarr.open(
        output_path,
        mode="w",
        shape=src.shape,
        chunks=src.chunks,
        dtype=src.dtype,
        fill_value=fill_value,
    )


def flip_zarr(input_path, output_path, axis_letter, axis_order="zyx", array_path=None):
    src = open_array(input_path, array_path=array_path)
    ndim = src.ndim
    axis = resolve_flip_axis(ndim, axis_letter, axis_order)

    dst = create_like(output_path, src)

    axis_size = src.shape[axis]
    total_chunks = 1
    for size, chunk in zip(src.shape, src.chunks):
        total_chunks *= -(-size // chunk)  # ceil division

    done = 0
    report_every = max(1, total_chunks // 20)

    for chunk_ranges in iter_chunk_ranges(src.shape, src.chunks):
        out_slices = tuple(slice(start, end) for start, end in chunk_ranges)

        src_slices = list(out_slices)
        start, end = chunk_ranges[axis]
        mirrored_start = axis_size - end
        mirrored_end = axis_size - start
        src_slices[axis] = slice(mirrored_start, mirrored_end)
        src_slices = tuple(src_slices)

        data = src[src_slices]
        data = np.flip(data, axis=axis)
        dst[out_slices] = data

        done += 1
        if done % report_every == 0 or done == total_chunks:
            pct = 100.0 * done / total_chunks
            print(f"\r  {done}/{total_chunks} chunks ({pct:.0f}%)", end="", file=sys.stderr)

    print(file=sys.stderr)
    return dst


def main():
    parser = argparse.ArgumentParser(
        description="Flip (mirror) a Zarr array along the x, y, or z axis.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("input_zarr", help="Path to the input .zarr array or group")
    parser.add_argument("output_zarr", help="Path to write the flipped .zarr array")
    parser.add_argument("axis", choices=["x", "y", "z", "X", "Y", "Z"],
                         help="Which axis to flip / mirror through")
    parser.add_argument(
        "--axis-order",
        default="zyx",
        help="Letters describing the array's trailing dimensions, in order "
             "(default: 'zyx', i.e. array is (..., z, y, x)).",
    )
    parser.add_argument(
        "--path",
        default=None,
        help="If the input is a Zarr group with multiple arrays, the path "
             "of the array to flip within that group (e.g. '0' for an "
             "OME-Zarr multiscale level).",
    )
    args = parser.parse_args()

    try:
        flip_zarr(
            args.input_zarr,
            args.output_zarr,
            args.axis,
            axis_order=args.axis_order,
            array_path=args.path,
        )
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Done. Wrote flipped array to '{args.output_zarr}'.")


if __name__ == "__main__":
    main()