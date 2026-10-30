#!/usr/bin/env bash
#
# For each rectangle: cut it out of the segment, fill it with simpaper10,
# and splice the filled patch back into the working copy.
#
# Rectangles are given in image coordinates (image origin = 0,0). They are
# shifted by the segment's own min 2D coordinates (as reported by gridfilter)
# to get coordinates in the segment's u-space.
#
# Usage: ./fill_rectangles.sh            (uses SEGMENT=patch_1)
#        SEGMENT=patch_2 ./fill_rectangles.sh

set -euo pipefail

SEGMENT="${SEGMENT:-c:/vesuvius/output/PHerc1667/patch_1}"

# One rectangle per line, as x,y,w,h. Blank lines and #-comments are ignored.
RECTANGLES='
12810,1462,836,413
# 13000,1500,200,300
'

# --- get the segment's min coordinates, e.g. "min: -9757, -2144" -------------
min_line=$(./gridfilter "${SEGMENT}.bin" | grep -E '^min:' | head -n 1 || true)
if [[ -z "$min_line" ]]; then
  echo "Error: could not find a 'min:' line in gridfilter output for ${SEGMENT}.bin" >&2
  exit 1
fi
# "min: -9757, -2144" -> "-9757 -2144"
read -r ox oy <<< "$(sed -E 's/^min:[[:space:]]*([^,[:space:]]+),[[:space:]]*(.+)$/\1 \2/' <<< "$min_line")"
echo "Segment min coords: ${ox}, ${oy}"

cp "${SEGMENT}.bin" "${SEGMENT}_working.bin"

while IFS=, read -r x y w h; do
  # skip blank lines and comments
  [[ -z "${x// /}" || "${x}" == \#* ]] && continue

  # Shift into segment coordinates: rx,ry = x+ox,y+oy ; rectangle = rx,ry,rx+w,ry+h
  # (awk so negative and non-integer values work)
  read -r rx ry rx2 ry2 <<< "$(awk -v x="$x" -v y="$y" -v w="$w" -v h="$h" -v ox="$ox" -v oy="$oy" \
    'BEGIN { rx = x + ox; ry = y + oy;
             printf "%.10g %.10g %.10g %.10g", rx, ry, rx + w, ry + h }')"
  rectangle="${rx},${ry},${rx2},${ry2}"

  echo "== ${SEGMENT}: image rect ${x},${y},${w},${h} -> ${rectangle}"

  ./gridfilter "${SEGMENT}.bin" "$rectangle" "${SEGMENT}_cut.bin"
  ./simpaper10_tmp hf "${SEGMENT}_cut.bin" "${SEGMENT}_cut_fill.bin"
  ./gridsplice "${SEGMENT}_working.bin" "${rx},${ry}" "${SEGMENT}_cut_fill.bin" "${SEGMENT}_working_out.bin"

  cp "${SEGMENT}_working_out.bin" "${SEGMENT}_working.bin"
done <<< "$RECTANGLES"

./render_from_zarr6_uint16 c:/vesuvius/scroll_data/zarrs/PHerc1667/volume/2 "${SEGMENT}_working.bin" -