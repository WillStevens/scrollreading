#!/bin/sh

set -x

TARGET_DIR=d:/pipelineOutputS4/all_s4_06

for N in {0..9}; do

./render_from_zarr6_uint16 d:/zarrs/s4/20231117161658.zarr/2 $TARGET_DIR/patch_$N.bin - -c $TARGET_DIR/patch_${N}_colours.csv

done