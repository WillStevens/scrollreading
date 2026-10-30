#!/bin/bash

set -x

for C in {0..9}; do

uv run --extra models python -m vesuvius.ink_detection.inference.infer   ../../ink-dataset/pherc1447/component_$C.zarr   checkpoints/ink_9um/hybrid_3d2d-seed42/step-075000.pth   predictions/pherc1447_component_$C.tif   --overlap 0.5 --blend-mode hann   --batch-size 16

done