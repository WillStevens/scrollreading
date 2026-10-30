#!/usr/bin/env python3
"""Convert a multi-frame surface-growth CSV into one 3D PNG per frame.

Input format: rows of  ux,uy,vx,vy,vz  (2D surface coords, 3D point coords),
with a line containing only FRAME marking the start of each new frame.

Example:
    python frames_to_png.py growth.csv --outdir frames --elev 30 --azim 45
"""
import argparse
import os
import sys

import matplotlib
matplotlib.use("Agg")  # headless rendering
import matplotlib.pyplot as plt
import matplotlib.tri as mtri
import numpy as np
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def read_frames(path):
    """Return a list of (N,5) float arrays, one per frame."""
    frames, rows = [], []

    def flush():
        if rows:
            frames.append(np.array(rows, dtype=float))
            rows.clear()

    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            if line.upper() == "FRAME":
                flush()
                continue
            parts = line.split(",")
            try:
                vals = [float(p) for p in parts]
            except ValueError:
                print(f"Warning: skipping unparseable line {lineno}: {line!r}",
                      file=sys.stderr)
                continue
            if len(vals) != 5:
                print(f"Warning: skipping line {lineno} (expected 5 values, "
                      f"got {len(vals)})", file=sys.stderr)
                continue
            rows.append(vals)
    flush()
    return frames


def bounding_box(frames):
    """Axis-aligned bounding box of all 3D points across all frames."""
    pts = np.vstack([fr[:, 2:5] for fr in frames])
    return pts.min(axis=0), pts.max(axis=0)


def render_frame(data, tri, lo, hi, elev, azim, out_path, dpi, title, cmap):
    fig = plt.figure(figsize=(8, 8))
    ax = fig.add_subplot(111, projection="3d")

    xyz = data[:, 2:5]
    verts = xyz[tri.triangles]                  # (T,3,3) triangle corners
    colour_vals = verts[:, :, 2].mean(axis=1)   # colour by mean height (vz)
    norm = plt.Normalize(lo[2], hi[2])
    poly = Poly3DCollection(verts, linewidths=0.2, edgecolors=(0, 0, 0, 0.3))
    poly.set_facecolor(plt.get_cmap(cmap)(norm(colour_vals)))
    ax.add_collection3d(poly)

    # Fixed limits from the global bounding box, so frames are comparable.
    pad = 0.02 * np.maximum(hi - lo, 1e-12)
    ax.set_xlim(lo[0] - pad[0], hi[0] + pad[0])
    ax.set_ylim(lo[1] - pad[1], hi[1] + pad[1])
    ax.set_zlim(lo[2] - pad[2], hi[2] + pad[2])
    # True proportions (guard against zero-extent axes, e.g. a flat sheet)
    ax.set_box_aspect(np.maximum(hi - lo, 1e-9 * max(np.max(hi - lo), 1.0)))

    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_zlabel("z")
    ax.view_init(elev=elev, azim=azim)
    ax.set_title(title)

    fig.savefig(out_path, dpi=dpi, bbox_inches="tight")
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="input CSV file")
    ap.add_argument("--outdir", default=".", help="output directory (default: .)")
    ap.add_argument("--prefix", default="frame", help="output filename prefix")
    ap.add_argument("--elev", "--axial", dest="elev", type=float, default=0.0,
                    help="axial (elevation) rotation in degrees (default 0)")
    ap.add_argument("--azim", dest="azim", type=float, default=0.0,
                    help="azimuthal rotation in degrees (default 0)")
    ap.add_argument("--dpi", type=int, default=100, help="image DPI (default 100)")
    ap.add_argument("--cmap", default="viridis", help="matplotlib colormap name")
    args = ap.parse_args()

    frames = read_frames(args.input)
    if not frames:
        sys.exit("No data found in input file.")

    lo, hi = bounding_box(frames)
    print(f"{len(frames)} frames. 3D bounding box:")
    for name, a, b in zip("xyz", lo, hi):
        print(f"  {name}: [{a:g}, {b:g}]")

    os.makedirs(args.outdir, exist_ok=True)
    width = max(4, len(str(len(frames) - 1)))

    tri, prev_uv = None, None
    for i, data in enumerate(frames):
        uv = data[:, 0:2]
        # Triangulate in the 2D parameter space; reuse if the points are unchanged.
        if prev_uv is None or uv.shape != prev_uv.shape or not np.array_equal(uv, prev_uv):
            tri = mtri.Triangulation(uv[:, 0], uv[:, 1])
            prev_uv = uv
        out = os.path.join(args.outdir, f"{args.prefix}_{i:0{width}d}.png")
        render_frame(data, tri, lo, hi, args.elev, args.azim, out, args.dpi,
                     f"Frame {i}", args.cmap)
        print(f"wrote {out}")


if __name__ == "__main__":
    main()