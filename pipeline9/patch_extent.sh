#!/bin/bash
# usage: ./overall_extent.sh /path/to/patch/directory

DIR="${1:-.}"

shopt -s nullglob
files=("$DIR"/patch_*.bin)
shopt -u nullglob

if [ ${#files[@]} -eq 0 ]; then
    echo "No patch_<n>.bin files found in $DIR" >&2
    exit 1
fi

for f in "${files[@]}"; do
    ../pipeline6/patch_extent "$f"
done | awk -F, '
    NR == 1 {
        xmin=$1; ymin=$2; zmin=$3; xmax=$4; ymax=$5; zmax=$6
        next
    }
    {
        if ($1 < xmin) xmin=$1
        if ($2 < ymin) ymin=$2
        if ($3 < zmin) zmin=$3
        if ($4 > xmax) xmax=$4
        if ($5 > ymax) ymax=$5
        if ($6 > zmax) zmax=$6
    }
    END {
        printf "%f,%f,%f,%f,%f,%f\n", xmin, ymin, zmin, xmax, ymax, zmax
    }
'