#!/bin/sh

# given components in the output dir, make corresponding surface volumes
OUTPUT_DIR=c:/vesuvius/output/PHerc0125
ZARR_DIR=c:/vesuvius/scroll_data/zarrs/PHerc0125/volume/0

for N in {0,1}; do
  cp $OUTPUT_DIR/patch_$N.bin $OUTPUT_DIR/surface_vol_tmp/patch_0.bin
  mkdir $OUTPUT_DIR/surface_vol/component_$N
  mkdir $OUTPUT_DIR/surface_vol/component_$N/interp
  mkdir $OUTPUT_DIR/surface_vol/component_$N/zarr3
  mkdir $OUTPUT_DIR/surface_vol/component_$N/zarr
  ./simpaper10 z $OUTPUT_DIR/surface_vol_tmp $OUTPUT_DIR/surface_vol/component_$N 14

  for M in {0..27}; do
    ./interpolate $OUTPUT_DIR/surface_vol/component_$N/patch_$M.bin $OUTPUT_DIR/surface_vol/component_$N/interp/patch_$M.bin
  done

  ./make_surface_vol $ZARR_DIR $OUTPUT_DIR/surface_vol/component_$N/interp $OUTPUT_DIR/surface_vol/component_$N/zarr3
  ./zarrconv.sh $OUTPUT_DIR/surface_vol/component_$N/zarr3 $OUTPUT_DIR/surface_vol/component_$N/zarr 1 0

done

