#!/usr/bin/env python3
"""Inspect one zarr chunk file and try to blosc-decompress it directly.

    python chunk_check.py c:\\vesuvius\\...\\surface\\0 28 3 14
"""
import os
import struct
import sys

import numcodecs.blosc as blosc

path, i, j, k = sys.argv[1], *map(int, sys.argv[2:5])

for meta in (".zarray", "zarr.json"):
    f = os.path.join(path, meta)
    if os.path.exists(f):
        print(f"--- {meta} ---\n{open(f).read()[:800]}\n")

candidates = [f"{i}.{j}.{k}", f"{i}/{j}/{k}", f"c/{i}/{j}/{k}", f"c/{i}.{j}.{k}"]
found = [c for c in candidates if os.path.isfile(os.path.join(path, c))]
if not found:
    sys.exit(f"No chunk file found. Tried: {candidates}\n"
             "(A chunk that is not stored is read as the fill value.)")
f = os.path.join(path, found[0])
data = open(f, "rb").read()
print(f"chunk file: {f}\nsize on disk: {len(data)} bytes")
print("first 16 bytes:", data[:16].hex(" "))

if len(data) >= 16:
    ver, verlz, flags, typesize, nbytes, blocksize, cbytes = struct.unpack("<BBBBIII", data[:16])
    print(f"blosc header: version {ver}, typesize {typesize}, "
          f"uncompressed {nbytes}, compressed size in header {cbytes}")
    if cbytes != len(data):
        print(f"  !! header says {cbytes} bytes but file has {len(data)} "
              f"-> file looks truncated/damaged" if cbytes > len(data) else
              f"  !! header says {cbytes} bytes but file has {len(data)}")

try:
    out = blosc.decompress(data)
    print(f"\nDECOMPRESS OK: {len(out)} bytes")
except Exception as e:
    print(f"\nDECOMPRESS FAILED: {type(e).__name__}: {e}")
