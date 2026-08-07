#!/bin/sh

for N in {18..40}; do

  cp d:/annealRuns/PHerc0139/annealState_out_$N.csv d:/pipelineOutput/manualBadPatch.csv
  ./simpaper10 v
  ./simpaper10 h
  ./simpaper10 f 30 <<HEREDOC
g
q
HEREDOC
  
  ./render_from_zarr6 d:/zarrs/PHerc0139/volume/2 d:/pipelineOutput/patch_0.bin - -c d:/pipelineOutput/patch_0_colours.csv

  cp d:/pipelineOutput/patch_0.tif d:/annealRuns/PHerc0139/patch_0_$N.tif


done