#!/bin/sh

set -x

for N in {0..27}; do
  ./interpolate d:/pipelineOutputPHerc1447/surface_vol/patch_$N.bin d:/pipelineOutputPHerc1447/surface_vol/interp/patch_$N.bin
done