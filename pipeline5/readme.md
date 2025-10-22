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

