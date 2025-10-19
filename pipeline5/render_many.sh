#!/bin/sh

for i in {15000..98000}
do
./render_from_zarr5 d:/pipelineOutputTry20/patch_$i.bin 0
done
