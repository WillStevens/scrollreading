#!/bin/sh

OUTPUT_DIR=c:/vesuvius/output/PHerc1667
ZARR_DIR=c:/vesuvius/scroll_data/zarrs/PHerc1667/volume/2

cp $OUTPUT_DIR/annealState_out.csv $OUTPUT_DIR/manualBadPatch.csv
./simpaper10 vm 50
./simpaper10 hm 50
./simpaper10 fm 30 50
rm $OUTPUT_DIR/manualBadPatch.csv

for N in {0..49}; do  
./render_from_zarr6_uint16 $ZARR_DIR $OUTPUT_DIR/patch_$N.bin - -c $OUTPUT_DIR/patch_${N}_colours.csv
done




