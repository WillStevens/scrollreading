#!/bin/sh

# given components in the output dir, make corresponding surface volumes
OUTPUT_DIR=c:/vesuvius/output/PHerc0125
ZARR_DIR=c:/vesuvius/scroll_data/zarrs/PHerc0125/volume/0

for N in {0,1}; do

  for M in {0..27}; do
    ./render_from_zarr6 $ZARR_DIR $OUTPUT_DIR/surface_vol/component_$N/interp/patch_$M.bin -
	mv $OUTPUT_DIR/surface_vol/component_$N/interp/patch_$M.tif $OUTPUT_DIR/surface_vol/component_$N/tif/$M.tif
  done

done

