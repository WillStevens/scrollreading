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
import itertools
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
def _use_blosc2_for_blosc():
    """numcodecs bundles the old blosc1 library, which cannot read chunks
    written by blosc2 ("error during blosc decompression: -1"). Blosc2 reads
    both formats, so decode 'blosc' chunks with it when it is installed."""
    try:
        import blosc2
    except ImportError:
        return  # fall back to numcodecs' own blosc; pip install blosc2 to fix
    import numcodecs
    from numcodecs.compat import ensure_contiguous_ndarray, ndarray_copy

    class Blosc2Compat(numcodecs.Blosc):
        def decode(self, buf, out=None):
            raw = blosc2.decompress(ensure_contiguous_ndarray(buf).tobytes())
            return ndarray_copy(np.frombuffer(raw, dtype="u1"), out)

    numcodecs.registry.register_codec(Blosc2Compat, codec_id="blosc")


def load_zarr_region(path, lo, hi, axes, spacing, origin, array_name=None,
                     margin=0, stride=1, skip_bad=False):
    """Read only the part of a 3D zarr that overlaps the world-space box
    lo..hi (x,y,z). Returns (bool volume, world origin of its corner), or
    (None, None) if there is no overlap."""
    import zarr

    _use_blosc2_for_blosc()
    arr = zarr.open(path, mode="r")
    if array_name:                      # path is a group
        arr = arr[array_name]
    spacing, origin = np.asarray(spacing, float), np.asarray(origin, float)

    # Where the whole zarr sits in world coordinates vs the surface, so a
    # spacing/origin/axes mismatch is obvious.
    ext = np.zeros((3, 2))
    for a, c in enumerate(axes):
        w = "xyz".index(c)
        ext[w] = sorted([origin[w], origin[w] + spacing[w] * arr.shape[a]])
    fmt = lambda b: ", ".join(f"{n} [{x:g}, {y:g}]" for n, (x, y) in zip("xyz", b))
    print(f"surface box (world):  {fmt(np.stack([lo, hi], axis=1))}")
    print(f"zarr extent (world):  {fmt(ext)}   <- from --zarr-axes/-spacing/-origin")

    start, stop = [], []
    for a, c in enumerate(axes):        # array axis a  <->  world axis w
        w = "xyz".index(c)
        i0, i1 = sorted(((np.array([lo[w], hi[w]]) - origin[w]) / spacing[w]))
        start.append(max(int(np.floor(i0)) - margin, 0))
        stop.append(min(int(np.ceil(i1)) + margin, arr.shape[a]))
    if any(b <= a for a, b in zip(start, stop)):
        return None, None

    # Read one chunk-sized piece at a time, serially, like zarr_peek.py does.
    # (One big slice makes zarr decode many chunks concurrently, which fails
    # with "blosc decompression: -1" on some setups.)
    vol = np.zeros([b - a for a, b in zip(start, stop)], dtype=bool)
    spans = []
    for a in range(3):
        c = arr.chunks[a]
        spans.append([(max(start[a], k * c), min(stop[a], (k + 1) * c))
                      for k in range(start[a] // c, (stop[a] - 1) // c + 1)])
    with zarr.config.set({"async.concurrency": 1, "threading.max_workers": 1}):
        for (x0, x1), (y0, y1), (z0, z1) in itertools.product(*spans):
            try:
                block = arr[x0:x1, y0:y1, z0:z1] != 0
            except Exception:
                cidx = [x0 // arr.chunks[0], y0 // arr.chunks[1], z0 // arr.chunks[2]]
                if skip_bad:
                    print(f"Warning: skipping unreadable zarr chunk {cidx} "
                          f"(treated as empty)", file=sys.stderr)
                    continue
                print(f"\nFAILED reading zarr chunk index {cidx} "
                      f"(array indices [{x0}:{x1}, {y0}:{y1}, {z0}:{z1}], "
                      f"chunk shape {arr.chunks}).\n"
                      f"If chunk_check.py shows the file starts with '05 01', it was written by "
                      f"blosc2: pip install blosc2\n"
                      f"Check just this chunk with:\n"
                      f"  python zarr_peek.py {path} {x0} {y0} {z0}"
                      f"{' --array ' + array_name if array_name else ''}\n",
                      file=sys.stderr)
                raise
            vol[x0 - start[0]:x1 - start[0],
                y0 - start[1]:y1 - start[1],
                z0 - start[2]:z1 - start[2]] = block
    print(f"zarr {arr.shape}: read region {[(a, b) for a, b in zip(start, stop)]} "
          f"-> {int(vol.sum())} set voxels")
    if not vol.any():
        print("Warning: the region under the surface contains no set voxels, so "
              "nothing will be drawn. Compare the two boxes above: if they don't "
              "line up, --zarr-axes/--zarr-spacing/--zarr-origin are wrong.",
              file=sys.stderr)

    new_origin = origin.copy()          # world position of the region's corner
    for a, c in enumerate(axes):
        w = "xyz".index(c)
        new_origin[w] += spacing[w] * start[a]
    return vol[::stride, ::stride, ::stride], new_origin


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
def make_triangulation(uv, max_edge_factor):
    """Delaunay-triangulate the 2D (ux,uy) points, then mask out triangles
    that bridge holes or concavities.

    Delaunay fills the convex hull, so triangles spanning a hole have
    unusually long edges in (ux,uy) space. Any triangle whose longest edge
    exceeds max_edge_factor * (median edge length) is removed.
    max_edge_factor <= 0 disables the filtering.
    """
    tri = mtri.Triangulation(uv[:, 0], uv[:, 1])
    if max_edge_factor > 0 and len(tri.triangles):
        p = uv[tri.triangles]                              # (T,3,2)
        e = np.stack([np.linalg.norm(p[:, 0] - p[:, 1], axis=1),
                      np.linalg.norm(p[:, 1] - p[:, 2], axis=1),
                      np.linalg.norm(p[:, 2] - p[:, 0], axis=1)], axis=1)
        tri.set_mask(e.max(axis=1) > max_edge_factor * np.median(e))
    return tri


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
    verts = xyz[tri.get_masked_triangles()]                  # (T,3,3) triangle corners
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
    ap.add_argument("--azim-step", type=float, default=0.0,
                    help="degrees to add to the azimuth on each successive frame "
                         "(default 0)")
    ap.add_argument("--elev-step", "--alt-step", dest="elev_step", type=float,
                    default=0.0,
                    help="degrees to add to the elevation (altitude) on each "
                         "successive frame (default 0)")
    ap.add_argument("--dpi", type=int, default=100, help="image DPI (default 100)")
    ap.add_argument("--cmap", default="viridis", help="matplotlib colormap name")

    ap.add_argument("--max-edge-factor", type=float, default=1.8,
                    help="remove surface triangles whose longest (ux,uy) edge is "
                         "more than this multiple of the median edge length, so "
                         "holes are not filled in (default 1.8; 0 = keep all)")

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
    zg.add_argument("--zarr-margin", type=int, default=0,
                    help="extra voxels to read around the surface's bounding box "
                         "(default 0)")
    zg.add_argument("--zarr-skip-bad-chunks", action="store_true",
                    help="treat unreadable chunks as empty instead of stopping")
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
        lo, hi = bounding_box(frames)   # surface extent decides what to read
        vol, corner = load_zarr_region(
            args.zarr, lo, hi, args.zarr_axes, args.zarr_spacing,
            args.zarr_origin, args.zarr_array, args.zarr_margin, args.zarr_stride,
            args.zarr_skip_bad_chunks)
        if vol is None:
            print("Warning: zarr does not overlap the surface; nothing to overlay.",
                  file=sys.stderr)
        else:
            spacing = np.asarray(args.zarr_spacing, float) * args.zarr_stride
            zarr_quads = volume_faces(vol, args.zarr_axes, spacing, corner)
            print(f"{len(zarr_quads)} exposed voxel faces")
            if len(zarr_quads) > 200_000:
                print("Warning: that many faces makes every frame slow to draw; "
                      "consider --zarr-stride 2 (or higher).", file=sys.stderr)

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
            tri = make_triangulation(uv, args.max_edge_factor)
            prev_uv = uv
        out = os.path.join(args.outdir, f"{args.prefix}_{i:0{width}d}.png")
        render_frame(data, tri, lo, hi, args.elev + i * args.elev_step,
                     args.azim + i * args.azim_step, out, args.dpi,
                     f"Frame {i}", args.cmap, zarr_quads,
                     args.zarr_color, args.zarr_alpha)
        print(f"wrote {out}")


if __name__ == "__main__":
    main()
