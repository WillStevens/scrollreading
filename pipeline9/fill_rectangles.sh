#!/usr/bin/env bash
#
# For each rectangle: cut it out of the segment, fill it with simpaper10,
# and splice the filled patch back into the working copy.
#
# Rectangles are given in image coordinates (image origin = 0,0). They are
# shifted by the segment's own min 2D coordinates (as reported by gridfilter)
# to get coordinates in the segment's u-space.
#
# Usage: ./fill_rectangles.sh <thread>           (uses SEGMENT=patch_1)
#        SEGMENT=patch_2 ./fill_rectangles.sh
# Use thread = 1,2,3 etc... in different shells to run several fills in parallel
# After all have done, use thread = 0 to collate

set -euo pipefail

TARGET=$1        # the parameter passed to the script
COUNTER=0

SEGMENT="${SEGMENT:-c:/vesuvius/output/PHerc1667/patch_1}"

# One rectangle per line, as x,y,w,h. Blank lines and #-comments are ignored.
RECTANGLES='
278,4092,398,215
681,4104,575,319
953,4020,405,136
1346,3960,523,373
3254,4049,389,302
3709,3696,364,395
3554,2961,956,413
4883,3145,760,432
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

  COUNTER=$((COUNTER + 1))

  # Shift into segment coordinates: rx,ry = x+ox,y+oy ; rectangle = rx,ry,rx+w,ry+h
  # (awk so negative and non-integer values work)
  read -r rx ry rx2 ry2 <<< "$(awk -v x="$x" -v y="$y" -v w="$w" -v h="$h" -v ox="$ox" -v oy="$oy" \
    'BEGIN { rx = x + ox; ry = y + oy;
             printf "%.10g %.10g %.10g %.10g", rx, ry, rx + w, ry + h }')"

  if [ "$COUNTER" -eq "$TARGET" ]; then

    rectangle="${rx},${ry},${rx2},${ry2}"

    echo "== ${SEGMENT}: image rect ${x},${y},${w},${h} -> ${rectangle}"

    ./gridfilter "${SEGMENT}.bin" "$rectangle" "${SEGMENT}_cut_${TARGET}.bin"
    ./simpaper10_tmp hf "${SEGMENT}_cut_${TARGET}.bin" "${SEGMENT}_cut_fill_${TARGET}.bin"
  fi
  
  
  if [ "$TARGET" -eq 0 ]; then
    ./gridsplice "${SEGMENT}_working.bin" "${rx},${ry}" "${SEGMENT}_cut_fill_${COUNTER}.bin" "${SEGMENT}_working_out.bin"
    cp "${SEGMENT}_working_out.bin" "${SEGMENT}_working.bin"
  fi
  
done <<< "$RECTANGLES"

if [ "$TARGET" -eq 0 ]; then
  ./render_from_zarr6_uint16 c:/vesuvius/scroll_data/zarrs/PHerc1667/volume/2 "${SEGMENT}_working.bin" -
fi

#./render_from_zarr6_uint16 c:/vesuvius/scroll_data/zarrs/PHerc1667/volume/2 "-c:/vesuvius/output/PHerc1667/patch_1_working.bin" -




