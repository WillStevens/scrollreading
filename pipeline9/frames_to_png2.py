#!/usr/bin/env python3
"""Convert a multi-frame surface-growth CSV into one 3D PNG per frame,
optionally overlaying a translucent binary 3D zarr volume.

Input format: rows of  ux,uy,vx,vy,vz  (2D surface coords, 3D point coords),
with a line containing only FRAME marking the start of each new frame.

Examples:
    python frames_to_png.py growth.csv --outdir frames --elev 30 --azim 45
    python frames_to_png.py growth.csv --zarr mask.zarr --zarr-axes zyx
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


# ----------------------------------------------------------------- CSV frames
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


# ----------------------------------------------------------------- zarr volume
def load_zarr_volume(path, array_name=None, stride=1):
    """Load a 3D zarr array as a boolean volume (nonzero -> True)."""
    import zarr  # imported lazily so zarr is only needed when used

    obj = zarr.open(path, mode="r")
    if hasattr(obj, "shape"):  # already an array
        arr = obj
    else:  # a group
        if array_name is None:
            names = sorted(k for k, _ in obj.arrays())
            if not names:
                sys.exit(f"No arrays found in zarr group {path!r}.")
            array_name = names[0]
            if len(names) > 1:
                print(f"Warning: zarr group has several arrays {names}; "
                      f"using {array_name!r} (override with --zarr-array)",
                      file=sys.stderr)
        arr = obj[array_name]
    if arr.ndim != 3:
        sys.exit(f"Zarr array must be 3D, got shape {arr.shape}.")
    vol = np.asarray(arr[::stride, ::stride, ::stride]) != 0
    print(f"zarr volume {tuple(arr.shape)} -> {vol.shape} after stride {stride}; "
          f"{int(vol.sum())} set voxels")
    return vol


def volume_faces(vol, axes, spacing, origin):
    """Exposed voxel faces as world-space quads, shape (F,4,3).

    Only faces between a set voxel and an empty one (or the array edge) are
    produced, which is all that is visible for a translucent solid and keeps
    the polygon count manageable.

    axes:    string like 'xyz'/'zyx'; character i = world axis of array axis i.
    spacing: voxel size in world x,y,z.
    origin:  world position of the corner of voxel [0,0,0], in x,y,z.
    """
    padded = np.pad(vol, 1)
    quad_uv = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], dtype=float)
    to_world = [("xyz".index(c)) for c in axes]  # array axis -> world axis
    spacing = np.asarray(spacing, float)
    origin = np.asarray(origin, float)

    out = []
    for a in range(3):
        b, c = [i for i in range(3) if i != a]
        for sign in (-1, 1):
            neighbour = np.roll(padded, -sign, axis=a)
            mask = (padded & ~neighbour)[1:-1, 1:-1, 1:-1]
            idx = np.argwhere(mask).astype(float)
            if len(idx) == 0:
                continue
            quad = np.repeat(idx[:, None, :], 4, axis=1)       # (n,4,3)
            quad[:, :, a] += 1.0 if sign > 0 else 0.0
            quad[:, :, b] += quad_uv[None, :, 0]
            quad[:, :, c] += quad_uv[None, :, 1]
            out.append(quad)
    if not out:
        return np.zeros((0, 4, 3))
    quads = np.concatenate(out)

    world = np.empty_like(quads)
    for arr_axis, w_axis in enumerate(to_world):
        world[:, :, w_axis] = origin[w_axis] + spacing[w_axis] * quads[:, :, arr_axis]
    return world


# ------------------------------------------------------------------- plotting
def bounding_box(frames, extra_pts=None):
    """Axis-aligned bounding box of all 3D points (and optional extra points)."""
    pts = [fr[:, 2:5] for fr in frames]
    if extra_pts is not None and len(extra_pts):
        pts.append(extra_pts.reshape(-1, 3))
    pts = np.vstack(pts)
    return pts.min(axis=0), pts.max(axis=0)


def render_frame(data, tri, lo, hi, elev, azim, out_path, dpi, title, cmap,
                 zarr_quads=None, zarr_color="red", zarr_alpha=0.25):
    fig = plt.figure(figsize=(8, 8))
    ax = fig.add_subplot(111, projection="3d")

    xyz = data[:, 2:5]
    verts = xyz[tri.triangles]                  # (T,3,3) triangle corners
    colour_vals = verts[:, :, 2].mean(axis=1)   # colour by mean height (vz)
    norm = plt.Normalize(lo[2], hi[2])
    poly = Poly3DCollection(verts, linewidths=0.2, edgecolors=(0, 0, 0, 0.3))
    poly.set_facecolor(plt.get_cmap(cmap)(norm(colour_vals)))
    ax.add_collection3d(poly)

    if zarr_quads is not None and len(zarr_quads):
        vol_poly = Poly3DCollection(zarr_quads, linewidths=0)
        vol_poly.set_facecolor(matplotlib.colors.to_rgba(zarr_color, zarr_alpha))
        vol_poly.set_edgecolor("none")
        ax.add_collection3d(vol_poly)

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

    zg = ap.add_argument_group("zarr overlay")
    zg.add_argument("--zarr", help="path to a 3D zarr array/group of 0/255 values "
                                   "to overlay translucently")
    zg.add_argument("--zarr-array", help="array name if --zarr is a group")
    zg.add_argument("--zarr-axes", default="xyz",
                    help="world axis for each zarr array axis, e.g. 'zyx' if the "
                         "array is stored z,y,x (default: xyz)")
    zg.add_argument("--zarr-spacing", type=float, nargs=3, default=[1, 1, 1],
                    metavar=("SX", "SY", "SZ"), help="voxel size in world x y z")
    zg.add_argument("--zarr-origin", type=float, nargs=3, default=[0, 0, 0],
                    metavar=("OX", "OY", "OZ"),
                    help="world position of the corner of voxel [0,0,0], x y z")
    zg.add_argument("--zarr-stride", type=int, default=1,
                    help="subsample the volume by this stride to speed up "
                         "rendering (voxel size is scaled to compensate)")
    zg.add_argument("--zarr-color", default="red", help="overlay colour (default red)")
    zg.add_argument("--zarr-alpha", type=float, default=0.25,
                    help="overlay opacity 0-1 (default 0.25)")
    args = ap.parse_args()

    if sorted(args.zarr_axes) != ["x", "y", "z"]:
        sys.exit("--zarr-axes must be a permutation of 'xyz'.")

    frames = read_frames(args.input)
    if not frames:
        sys.exit("No data found in input file.")

    zarr_quads = None
    if args.zarr:
        vol = load_zarr_volume(args.zarr, args.zarr_array, args.zarr_stride)
        spacing = np.asarray(args.zarr_spacing, float) * args.zarr_stride
        zarr_quads = volume_faces(vol, args.zarr_axes, spacing, args.zarr_origin)
        print(f"{len(zarr_quads)} exposed voxel faces")

    lo, hi = bounding_box(frames, zarr_quads)
    print(f"{len(frames)} frames. 3D bounding box (surface"
          f"{' + zarr' if zarr_quads is not None else ''}):")
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
                     f"Frame {i}", args.cmap, zarr_quads,
                     args.zarr_color, args.zarr_alpha)
        print(f"wrote {out}")


if __name__ == "__main__":
    main()