To download and run this pipeline, follow these steps:

1. Download the vector field Zarr from here (this is from a 3072 by 3072 by 4096 region of scroll1A):
2. Download all or part of the scroll1 OME Zarr. This pipeline only uses the full resolution subfolder.
3. Do a sparse git clone to get just this pipeline5 folder:
4. Set directory locations for the two Zarrs in parameters.json
5. Set the output directory location (all patches and other files that the pipeline produces will be placed here)
6. Run build_pipeline5.sh
7. The pipeline is now ready to run: sp_pipeline4.py
8. After it has run for a while, interupt it.
9. The pipeline will have produced a file patchCoords.txt - this contains the relationships between all overlapping patches
10. Run patchsprings.py (still experimental) to refine the relationships between patches. This will produce a file patchPositions.txt
11. You can render the surface that it produced using: render_from_zarr5 ..., using the patchPositions.txt

The main problems to be resolved with this pipeline are that the positions in patchPositions.txt aren't always correct, and render_from_zarr5 is only approximate - in future it will need to somehow treat patchPositions.txt like a distortion map to be applied to patches. There also seems to be a problem where more and more patches are rejected as the grown surfaces gets larger.

The area-per-second performance of the patch-growing part of the pipeline (simpaper9) is similar to VC3Ds vc_grow_seg_from_seed. I tried both from the same seed point on the same hardware (single threaded, no GPU): vc_grow_seg_from_seed ran at an average speed of 50mm^2/s, simpaper9 ran at 33mm^2/s. simpaper9 has a higher quadmesh resolution (4 voxels per quadmesh coordinate) than vc_grow_seg_from_seed (20 voxels), so simpaper9 produces a larger number of quadmesh points per second (about 40000/s) than vc_grow_seg_from_seed (which must be about 2000/s). I have not tried running simpaper9 at a resolution of more than 6 voxels per quadmesh coordinate.

The rest of the pipeline is slower - simpaper9 isn't the rate-limiting factor. There are losses due to a high degree of patch overlap, rejection of patches that don't seem to align, and due to the fact that the rest of the pipeline after simpaper9 runs at full resolution - 1 voxel per quadmesh point. It can produce 100cm"2 in about 8 hours (i.e. 0.35 mm^2/s). All of these things could probably be improved.

If you want to make your own vector field ZARR rather than download it, use the programs:
papervectorfield_closest3.c   TODO - this needs to be updated to user parameters.h for every parameter
vectorfieldsmooth_64.c        TODO - this needs to be updated to user parameters.h for every parameter

Invoke them as follows:

papervectorfield_closest3 0 4096
vectorfieldsmooth_64 0 4096

(Assuming you want to process slices 0 to 4096 starting at VOL_OFFSET_Z in parameters.json)
These are single threaded programs. You can make use of multiple CPU cores by running separate processes for different parts of the Zarr, e.g.
papervectorfield_closest3 0 2048
papervectorfield_closest3 2048 4096

