#!/usr/bin/env bash
set -euo pipefail

dir="${1:?Usage: $0 <directory>}"

max_x=-1
max_y=-1
max_z=-1

for f in "$dir"/*; do
    name=$(basename "$f")
    if [[ "$name" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        z="${BASH_REMATCH[1]}"
        y="${BASH_REMATCH[2]}"
        x="${BASH_REMATCH[3]}"
        (( x > max_x )) && max_x=$x
        (( y > max_y )) && max_y=$y
        (( z > max_z )) && max_z=$z
    fi
done

echo "Max x: $max_x"
echo "Max y: $max_y"
echo "Max z: $max_z"