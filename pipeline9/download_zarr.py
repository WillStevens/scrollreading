"""
Download a Zarr store from an S3-compatible HTTP endpoint.

Supports:
  - Optional bounding box to download only the chunks that overlap a region
  - Skip files that are already present on disk
  - Parallel downloads with retry/back-off

Usage:
    python download_zarr.py [output_folder] [options]

Examples:
    # Full download
    python download_zarr.py C:\\data

    # Only chunks overlapping rows 1000-2000, cols 500-1500 (2-D example)
    python download_zarr.py C:\\data --bbox 1000,500 2000,1500

    # 3-D bounding box  (min z,y,x  max z,y,x)
    python download_zarr.py C:\\data --bbox 0,1000,500 10,2000,1500

Requirements:
    pip install requests tqdm
"""

import json
import sys
import argparse
import time
import xml.etree.ElementTree as ET
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from threading import Lock

import requests
from tqdm import tqdm

# ── Configuration ────────────────────────────────────────────────────────────

BASE_URL = "https://vesuvius-challenge-open-data.s3.us-east-1.amazonaws.com"
ZARR_PREFIX = (
    "PHerc0125/volumes/20250821151825-9.362um-1.2m-113keV-masked.zarr/0/"
)

MAX_WORKERS   = 8
CHUNK_SIZE    = 1024 * 256   # 256 KB streaming buffer
MAX_RETRIES   = 5
RETRY_BACKOFF = 2.0          # seconds; doubles each retry

# ─────────────────────────────────────────────────────────────────────────────

BoundingBox = tuple[tuple[int, ...], tuple[int, ...]]   # (min_coords, max_coords)


def build_url(*parts: str) -> str:
    return "/".join(s.strip("/") for s in parts)


# ── S3 listing ───────────────────────────────────────────────────────────────

def list_keys(base_url: str, prefix: str) -> list[str]:
    """Return every S3 key under *prefix* using the XML ListObjectsV2 API."""
    bucket_url = base_url.rstrip("/") + "/"
    keys: list[str] = []
    continuation_token: str | None = None

    print(f"Listing objects under: {prefix}")
    while True:
        params: dict = {"list-type": "2", "prefix": prefix, "max-keys": "1000"}
        if continuation_token:
            params["continuation-token"] = continuation_token

        resp = requests.get(bucket_url, params=params, timeout=30)
        resp.raise_for_status()

        root = ET.fromstring(resp.text)
        ns = {"s3": "http://s3.amazonaws.com/doc/2006-03-01/"}

        for content in root.findall("s3:Contents", ns):
            key_el = content.find("s3:Key", ns)
            if key_el is not None and key_el.text:
                keys.append(key_el.text)

        is_truncated = root.findtext("s3:IsTruncated", namespaces=ns)
        if is_truncated and is_truncated.lower() == "true":
            continuation_token = root.findtext(
                "s3:NextContinuationToken", namespaces=ns
            )
        else:
            break

    print(f"Found {len(keys)} object(s).")
    return keys


# ── Zarr metadata ─────────────────────────────────────────────────────────────

def fetch_zarray(base_url: str, zarr_prefix: str, array_path: str = "") -> dict | None:
    """
    Fetch and return the .zarray JSON for an array within the zarr store.
    *array_path* is the sub-path inside the zarr root (empty = root array).
    Returns None if the request fails (e.g. not an array node).
    """
    parts = [base_url, zarr_prefix]
    if array_path:
        parts.append(array_path)
    parts.append(".zarray")
    url = build_url(*parts)
    try:
        resp = requests.get(url, timeout=15)
        if resp.status_code == 200:
            return resp.json()
    except Exception:
        pass
    return None


def find_arrays(keys: list[str], zarr_prefix: str) -> dict[str, dict]:
    """
    Discover all arrays inside the zarr by scanning for .zarray files in the
    listed keys.  Returns {relative_array_path: zarray_metadata}.
    """
    arrays: dict[str, dict] = {}
    for key in keys:
        # key relative to zarr root
        rel = key[len(zarr_prefix):].lstrip("/")
        if rel.endswith(".zarray"):
            array_rel = rel[: -len(".zarray")].rstrip("/")
            # fetch the actual JSON (the listing doesn't give us content)
            meta = fetch_zarray(BASE_URL, zarr_prefix, array_rel)
            if meta:
                arrays[array_rel] = meta
    return arrays


# ── Chunk key parsing ─────────────────────────────────────────────────────────

def is_metadata_key(rel: str) -> bool:
    """True for .zattrs, .zarray, .zgroup, .zmetadata, and similar."""
    basename = rel.split("/")[-1] if "/" in rel else rel
    return basename.startswith(".z")


def parse_chunk_coords(rel_from_array: str) -> tuple[int, ...] | None:
    """
    Parse a zarr chunk key (relative to its array directory) into a tuple of
    integer chunk indices.  Returns None if the key is not a chunk.

    Zarr chunk key formats:
      v2 flat store    "0.1.2"       → (0, 1, 2)   dot-separated
      v2 nested store  "0/1/2"       → (0, 1, 2)   slash-separated integers
      v3              "c/0/1/2"      → (0, 1, 2)   slash-separated under c/

    Some zarrs mix conventions; we try each in order and return the first
    result whose every component is a non-negative integer.
    """
    s = rel_from_array.lstrip("/")

    # Zarr v3: must start with "c/"
    if s.startswith("c/"):
        parts = s[2:].split("/")
        try:
            coords = tuple(int(p) for p in parts if p)
            return coords if coords else None
        except ValueError:
            return None

    # Zarr v2 nested store: all path components are integers, e.g. "0/1/2"
    slash_parts = s.split("/")
    try:
        coords = tuple(int(p) for p in slash_parts if p)
        if coords:
            return coords
    except ValueError:
        pass

    # Zarr v2 flat store: single filename with dot-separated integers, e.g. "0.1.2"
    dot_parts = s.split(".")
    try:
        coords = tuple(int(p) for p in dot_parts if p)
        if coords:
            return coords
    except ValueError:
        pass

    return None


def chunk_overlaps_bbox(
    chunk_coords: tuple[int, ...],
    chunk_shape: list[int],
    bbox_min: tuple[int, ...],
    bbox_max: tuple[int, ...],
) -> bool:
    """
    Return True if the chunk at *chunk_coords* (in chunk-index space) overlaps
    the bounding box expressed in array-element coordinates [bbox_min, bbox_max).
    """
    ndim = len(chunk_coords)
    if len(bbox_min) != ndim or len(bbox_max) != ndim:
        # Dimension mismatch: the chunk coords don't match the bbox dimensionality.
        # This most likely means parse_chunk_coords got the wrong path — exclude
        # rather than silently include everything (which defeats the bbox filter).
        return False

    for dim in range(ndim):
        chunk_start = chunk_coords[dim] * chunk_shape[dim]
        chunk_end   = chunk_start + chunk_shape[dim]
        if chunk_end <= bbox_min[dim] or chunk_start >= bbox_max[dim]:
            return False
    return True


# ── Filtering ─────────────────────────────────────────────────────────────────

def filter_keys(
    keys: list[str],
    zarr_prefix: str,
    arrays: dict[str, dict],
    bbox: BoundingBox | None,
) -> list[str]:
    """
    Given all S3 keys, return only those that should be downloaded:
      - Always include metadata files (.z*)
      - If bbox is None, include all chunk files
      - If bbox is set, include only chunks that overlap the bbox
    """
    if bbox is None:
        return keys

    bbox_min, bbox_max = bbox
    kept: list[str] = []

    for key in keys:
        rel = key[len(zarr_prefix):].lstrip("/")

        if is_metadata_key(rel.split("/")[-1] if "/" in rel else rel):
            kept.append(key)
            continue

        # Determine which array this key belongs to (longest prefix match wins).
        # The root array ("") matches anything not claimed by a deeper array.
        matched_array_rel: str | None = None
        matched_meta: dict | None = None
        for array_rel, meta in arrays.items():
            if array_rel == "":
                # Root array: matches only if nothing longer matches — handled below
                continue
            prefix = array_rel + "/"
            if rel.startswith(prefix):
                if (
                    matched_array_rel is None
                    or len(array_rel) > len(matched_array_rel)
                ):
                    matched_array_rel = array_rel
                    matched_meta = meta

        # Fall back to root array only if no deeper array claimed this key
        if matched_meta is None and "" in arrays:
            matched_array_rel = ""
            matched_meta = arrays[""]

        if matched_meta is None:
            # Unknown structure – include to be safe
            kept.append(key)
            continue

        chunk_shape: list[int] = matched_meta["chunks"]
        array_rel_prefix = (matched_array_rel + "/") if matched_array_rel else ""
        rel_from_array = rel[len(array_rel_prefix):]

        coords = parse_chunk_coords(rel_from_array)
        if coords is None:
            # Not a chunk (e.g. some other metadata) – include it
            kept.append(key)
            continue

        if chunk_overlaps_bbox(coords, chunk_shape, bbox_min, bbox_max):
            kept.append(key)

    return kept


# ── Download ──────────────────────────────────────────────────────────────────

def download_file(
    url: str,
    dest: Path,
    progress_lock: Lock,
    pbar: tqdm,
) -> str:
    """Download *url* to *dest* with retry logic.  Returns 'ok' or 'skipped'."""
    if dest.exists():
        with progress_lock:
            pbar.update(1)
        return "skipped"

    dest.parent.mkdir(parents=True, exist_ok=True)

    for attempt in range(1, MAX_RETRIES + 1):
        try:
            with requests.get(url, stream=True, timeout=60) as resp:
                resp.raise_for_status()
                # Write to a temp file first, then rename – avoids partial files
                tmp = dest.with_suffix(dest.suffix + ".part")
                try:
                    with open(tmp, "wb") as fh:
                        for chunk in resp.iter_content(chunk_size=CHUNK_SIZE):
                            if chunk:
                                fh.write(chunk)
                    tmp.replace(dest)
                except Exception:
                    tmp.unlink(missing_ok=True)
                    raise
            with progress_lock:
                pbar.update(1)
            return "ok"
        except Exception as exc:
            if attempt == MAX_RETRIES:
                raise RuntimeError(
                    f"Failed after {MAX_RETRIES} attempts: {exc}"
                ) from exc
            time.sleep(RETRY_BACKOFF * (2 ** (attempt - 1)))

    return "ok"  # unreachable, satisfies type checker


def download_zarr(output_dir: Path, bbox: BoundingBox | None) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)

    all_keys = list_keys(BASE_URL, ZARR_PREFIX)
    if not all_keys:
        print("No objects found. Check the prefix and bucket URL.")
        sys.exit(1)

    # Discover array metadata (needed for bbox filtering)
    if bbox is not None:
        print("Reading array metadata to resolve chunk coordinates…")
        arrays = find_arrays(all_keys, ZARR_PREFIX)
        if not arrays:
            print(
                "Warning: could not find any .zarray metadata. "
                "Downloading everything."
            )
            bbox = None
            arrays = {}
        else:
            for arr_rel, meta in sorted(arrays.items()):
                label = arr_rel if arr_rel else "(root)"
                print(
                    f"  Array {label}: shape={meta.get('shape')}, "
                    f"chunks={meta.get('chunks')}, dtype={meta.get('dtype')}"
                )
    else:
        arrays = {}

    # Filter by bounding box
    keys = filter_keys(all_keys, ZARR_PREFIX, arrays, bbox)

    if bbox is not None:
        skipped_count = len(all_keys) - len(keys)
        print(
            f"Bounding box filter: {len(keys)} file(s) selected "
            f"({skipped_count} outside bbox, excluded)."
        )

    # Build task list: (url, local dest path)
    tasks: list[tuple[str, Path]] = []
    for key in keys:
        url  = build_url(BASE_URL, key)
        dest = output_dir / Path(*key.split("/"))   # Windows-safe path split
        tasks.append((url, dest))

    already = sum(1 for _, dest in tasks if dest.exists())
    todo    = len(tasks) - already
    print(
        f"\n{len(tasks)} file(s) to consider: "
        f"{todo} to download, {already} already on disk (will skip)."
    )
    print(f"Output → {output_dir}")
    print(f"Threads → {MAX_WORKERS}\n")

    lock   = Lock()
    errors: list[str] = []
    counts = {"ok": 0, "skipped": 0}

    with tqdm(total=len(tasks), unit="file", dynamic_ncols=True) as pbar:
        with ThreadPoolExecutor(max_workers=MAX_WORKERS) as pool:
            futures = {
                pool.submit(download_file, url, dest, lock, pbar): (url, dest)
                for url, dest in tasks
            }
            for future in as_completed(futures):
                url, dest = futures[future]
                try:
                    result = future.result()
                    with lock:
                        counts[result] += 1
                except Exception as exc:
                    errors.append(f"{url}: {exc}")
                    with lock:
                        pbar.write(f"[ERROR] {exc}")

    print(
        f"\nDone — {counts['ok']} downloaded, "
        f"{counts['skipped']} skipped (already existed)."
    )
    if errors:
        print(f"\n{len(errors)} file(s) failed:")
        for e in errors:
            print(f"  • {e}")
        sys.exit(1)


# ── CLI ───────────────────────────────────────────────────────────────────────

def parse_coord(s: str) -> tuple[int, ...]:
    try:
        return tuple(int(v) for v in s.split(","))
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"Expected comma-separated integers, got: {s!r}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Download a Zarr store from S3 via HTTP.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Bounding box (--bbox) is specified in array-element coordinates:

  --bbox MIN_COORDS MAX_COORDS

where MIN_COORDS and MAX_COORDS are comma-separated integers, one per
dimension, in the same axis order as the zarr array (e.g. z,y,x).

Examples:
  # 2-D: rows 1000-2000, cols 500-1500
  --bbox 1000,500 2000,1500

  # 3-D: z 0-10, y 1000-2000, x 500-1500
  --bbox 0,1000,500 10,2000,1500
""",
    )
    parser.add_argument(
        "output_folder",
        nargs="?",
        default=None,
        help=(
            "Destination folder. "
            "Defaults to the current directory (full S3 key path mirrored inside)."
        ),
    )
    parser.add_argument(
        "--bbox",
        nargs=2,
        metavar=("MIN_COORDS", "MAX_COORDS"),
        type=parse_coord,
        default=None,
        help=(
            "Optional bounding box in array-element coordinates. "
            "Provide two comma-separated coordinate strings: min then max. "
            "Chunks that do not overlap this region are skipped."
        ),
    )
    args = parser.parse_args()

    output_dir = Path(args.output_folder) if args.output_folder else Path.cwd()

    bbox: BoundingBox | None = None
    if args.bbox:
        bbox_min, bbox_max = args.bbox
        if len(bbox_min) != len(bbox_max):
            parser.error("--bbox min and max must have the same number of dimensions.")
        bbox = (bbox_min, bbox_max)
        print(f"Bounding box: min={bbox_min}  max={bbox_max}")

    download_zarr(output_dir, bbox)


if __name__ == "__main__":
    main()