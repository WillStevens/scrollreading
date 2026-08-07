#!/usr/bin/env python3
"""
Count how many times each integer occurs across all annealState_N.csv files
in a directory, and print them sorted by frequency (most frequent first).

Usage:
    python count_frequencies.py [directory]

If no directory is given, the current directory is used.
"""

import sys
import glob
import os
import csv
from collections import Counter


def find_annealstate_files(directory):
    pattern = os.path.join(directory, "annealState_*.csv")
    return glob.glob(pattern)


def count_integers(files):
    counts = Counter()
    for filepath in files:
        with open(filepath, newline="") as f:
            reader = csv.reader(f)
            for row in reader:
                for field in row:
                    field = field.strip()
                    if not field:
                        continue
                    try:
                        value = int(field)
                    except ValueError:
                        continue
                    counts[value] += 1
    return counts


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else "."

    files = find_annealstate_files(directory)
    if not files:
        print(f"No annealState_N.csv files found in '{directory}'", file=sys.stderr)
        sys.exit(1)

    counts = count_integers(files)

    # Sort by frequency descending, then by value ascending for ties
    for value, freq in sorted(counts.items(), key=lambda item: (-item[1], item[0])):
        print(f"{value},{freq}")


if __name__ == "__main__":
    main()