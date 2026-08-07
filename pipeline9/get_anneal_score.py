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


def find_annealstats_files(directory):
    pattern = os.path.join(directory, "annealStats_*.csv")
    return glob.glob(pattern)


def find_max_scores(files):
    for filepath in files:
        with open(filepath, newline="") as f:
            reader = csv.reader(f)
            maxScore = -1000000000
            for row in reader:
                score = row[0].strip()
                if not score:
                    continue
                try:
                    score = int(score)
                except ValueError:
                    continue
                if score > maxScore:
                    maxScore = score
        print(filepath + " " + str(maxScore))


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else "."

    files = find_annealstats_files(directory)
    if not files:
        print(f"No annealStats_N.csv files found in '{directory}'", file=sys.stderr)
        sys.exit(1)

    find_max_scores(files)



if __name__ == "__main__":
    main()