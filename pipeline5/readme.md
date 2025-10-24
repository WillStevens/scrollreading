To download and run this pipeline, follow these steps:

1. Install the blosc2 and libtiff-dev C libraries.
2. Download the vector field Zarr from here (this is from a 3072 by 3072 by 4096 region of scroll1A): https://dl.ash2txt.org/community-uploads/will/pvfs_2025_10_19.zarr.7z
3. Download all or part of the scroll1A OME Zarr from https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/ (This pipeline only uses the full resolution subfolder of the OME Zarr).
4. Do a sparse git clone to get just this pipeline5 folder:
   ```
   git clone --no-checkout https://github.com/WillStevens/scrollreading.git
   cd scrollreading
   git sparse-checkout init --no-cone
   git sparse-checkout set pipeline5
   git checkout @
   ```
5. Set directory locations for the two Zarrs in parameters.json, e.g.: 	`"VOLUME_ZARR" : "d:/zarrs/54keV_7.91um_Scroll1A.zarr/0/",
	"VECTORFIELD_ZARR" : "d:/pvfs_2025_10_19.zarr"`
6. Set the output directory location (all patches and other files that the pipeline produces will be placed here), e.g.: `"OUTPUT_DIR" : "d:/pipelineOutput"`
7. Run `bash build_pipeline5.sh`. This should only take a few seconds.
8. The pipeline is now ready to run, run it using: `python sp_pipeline4.py`
9. After it has run for a few minutes, interupt it (Ctrl-C).
10. The pipeline will have produced:
    - surface.bp - a chunked compressed data structure containing the patches at full resolution.
    - boundary.bp - a representation of the boundary used by the pipeline to work out the next seed point.
    - *.bin - all of the patches in a binary qx,qy,vx,vy,vz format (q=quadmesh, v=volume). These are in low resolution (4-voxels per quadmesh point).
    -  patchCoords.txt - this contains the relationships between all overlapping patches, represented as an affine transformation, along with the variance of the transformations sampled.
11. Run `python patchsprings.py` (still experimental) to refine the relationships between patches. This will produce a file patchPositions.txt
12. You can render the surface that it produced using: `render_from_zarr5 <value of VOLUME_ZARR> <value of OUTPUT_DIR>/surface.bp <value of OUTPUT_DIR>/patchPositions.txt`

The pipeline has been run on windows (using cygwin) and on linux.

The main problems to be resolved with this pipeline are that the positions in patchPositions.txt sometimes get misled by occasional badly-aligned patches. Also render_from_zarr5 is only approximate and doesn't stretch or distort patches - in future it will need to somehow treat patchPositions.txt like a distortion map to be applied to patches. There also seems to be a problem where more and more patches are rejected as the grown surfaces gets larger.

The area-per-second performance of the patch-growing part of the pipeline (simpaper9) is similar to VC3Ds vc_grow_seg_from_seed. I tried both from the same seed point on the same hardware (single threaded, no GPU): vc_grow_seg_from_seed ran at an average speed of 50mm^2/s, simpaper9 ran at 33mm^2/s. simpaper9 has a higher quadmesh resolution (4 voxels per quadmesh coordinate) than vc_grow_seg_from_seed (20 voxels), so simpaper9 produces a larger number of quadmesh points per second (about 40000/s) than vc_grow_seg_from_seed (which must be about 2000/s). I have not tried running simpaper9 at a resolution lower than 6 voxels per quadmesh coordinate.

The rest of the pipeline is slower - simpaper9 isn't the rate-limiting factor. There are losses due to a high degree of patch overlap, rejection of patches that don't seem to align, and due to the fact that the rest of the pipeline after simpaper9 runs at full resolution - 1 voxel per quadmesh point. It can produce 100cm"2 in about 8 hours (i.e. 0.35 mm^2/s). All of these things could probably be improved.

In future I'm going to explore what can be done to increase the size of patches that the patch growing program simpaper9 can produce, or alternatively force new patches to follow existing patches where they overlap - I beleive that this will reduce patch misalignment.

If you want to make your own vector field ZARR rather than download it, use the programs: papervectorfield_closest3.c and vectorfieldsmooth_64.c

You will need to download a surface prediction zarr, and also have space for a temporary zarr. This is used for the output of papervectorfield_closest3.c and the input of vectorfieldsmooth64.c.

Invoke them as follows:
```
papervectorfield_closest3 <surface zarr> <temporary zarr> 0 4096
vectorfieldsmooth_64 <temporary> <vectorfield zarr> 0 4096
```
(Assuming you want to process slices 0 to 4096 starting at VOL_OFFSET_Z in parameters.json)

These are single threaded programs. You can make use of multiple CPU cores by running separate processes for different parts of the Zarr, e.g.
```
papervectorfield_closest3 <surface zarr> <temporary zarr> 0 2048
papervectorfield_closest3 <surface zarr> <temporary zarr> 2048 4096
```
N.B. The zarr this produces (like pvfs_2025_10_19.zarr) doesn't open with pythons zarr library - I've haven't yet worked out why.
