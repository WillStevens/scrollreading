#!/bin/sh

for i in {0..1}
do
../pipeline6/render_from_zarr5 d:/zarrs/s4/20231117161658.zarr/0 patches/patch_$i.bin -
done
