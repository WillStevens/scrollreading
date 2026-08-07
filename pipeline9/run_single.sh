#!/bin/sh

# using whatever is in badpatches.csv and manualBadPatch.csv, render the surface

./simpaper10 v
./simpaper10 h
./simpaper10 f 30 <<HEREDOC
g
q
HEREDOC
  
./render_from_zarr6 d:/zarrs/PHerc0139/volume/2 d:/pipelineOutput/patch_0.bin - -c d:/pipelineOutput/patch_0_colours.csv

