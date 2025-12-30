This pipeline (pipeline6) is derived from pipeline5, but targets Scroll 4.
To download and run this pipeline, follow these steps:

1. Install the blosc2 and libtiff-dev C libraries.
2. Download a surface prediction zarr from here: (https://dl.ash2txt.org/community-uploads/bruniss/scrolls/s4/surfaces/s4_059_medial_ome.zarr/)
3. Download all or part of the scroll 4 OME 7.91um resolution zarr from (https://dl.ash2txt.org/full-scrolls/Scroll4/PHerc1667.volpkg/volumes_zarr/20231117161658.zarr/0) (This pipeline only uses the full resolution subfolder of the OME Zarr).
4. Do a sparse git clone to get just this pipeline5 folder:
   ```
   git clone --no-checkout https://github.com/WillStevens/scrollreading.git
   cd scrollreading
   git sparse-checkout init --no-cone
   git sparse-checkout set pipeline6
   git checkout @
   ```
5. Set directory locations for the two Zarrs obtained above in parameters.json: "VOLUME_ZARR" and "SURFACE_ZARR"
6. Set the output directory location (all patches and other files that the pipeline produces will be placed here), e.g.: `"OUTPUT_DIR" : "d:/pipelineOutput"`
7. Run `bash build_pipeline6.sh`. This should only take a few seconds.
8. Make your own vector field zarr using the programs: papervectorfield_closest3.c and vectorfieldsmooth_64.c

These use the surface prediction zarr from step 2. You will need to have space for a temporary zarr VF_TEMP_ZARR. This is used for the output of papervectorfield_closest3.c and the input of vectorfieldsmooth_64.c and the vectorfield zarr VECTORFIELD_ZARR that vectorfieldsmooth_64.c outputs.

Invoke them as follows:
```
papervectorfield_closest3 <surface zarr> <temporary zarr> 0 4096
vectorfieldsmooth_64 <temporary zarr> <vectorfield zarr> 0 4096
```
(Assuming you want to process slices 0 to 4096 starting at VOL_OFFSET_Z in parameters.json)

These are single threaded programs. You can make use of multiple CPU cores by running separate processes for different parts of the Zarr, e.g.
```
papervectorfield_closest3 <surface zarr> <temporary zarr> 0 2048
papervectorfield_closest3 <surface zarr> <temporary zarr> 2048 4096
```
N.B. The zarr this produces doesn't open with pythons zarr library - I've haven't yet worked out why.

9. The pipeline is now ready to run, run it using: `python sp_pipeline4.py`
10. After it has run for a few minutes, interupt it (Ctrl-C).
11. The pipeline will have produced:
    - surface.bp - a chunked compressed data structure containing the patches at full resolution.
    - boundary.bp - a representation of the boundary used by the pipeline to work out the next seed point.
    - *.bin - all of the patches in a binary qx,qy,vx,vy,vz format (q=quadmesh, v=volume). These are in low resolution (4-voxels per quadmesh point).
    -  patchCoords.txt - this contains the relationships between all overlapping patches, represented as an affine transformation, along with the variance of the transformations sampled.
12. Run `python patchsprings.py` (still experimental) to refine the relationships between patches. This will produce a file patchPositions.txt
13. You can render the surface that it produced using: `render_from_zarr5 <value of VOLUME_ZARR> <value of OUTPUT_DIR>/surface.bp <value of OUTPUT_DIR>/patchPositions.txt`. This will output a file surface.tif in the output directory.

The pipeline has been run on windows (using cygwin) and on linux.

The main problems to be resolved with this pipeline are that the positions in patchPositions.txt sometimes get misled by occasional badly-aligned patches. Also render_from_zarr5 is only approximate and doesn't stretch or distort patches - in future it will need to somehow treat patchPositions.txt like a distortion map to be applied to patches. There also seems to be a problem where more and more patches are rejected as the grown surfaces gets larger.

This version of the pipeline (pipeline6) introduces several new programs that attempt to identify patches that result in sheet switching and misalignement. These programs are:

- find_bad_patches.py : top level program for finding bad patches. Calls some of the other programs below, writes all outputs to the specified working directory.
- make_neighbourmap.py : output patch neighbour relationships into the specified working directory.
- find_mismatch.cpp : given a directory containing .bin patches and a <patchpositions> file, look for 2D points that map to different 3D points farther apart than the specified distance threshhold. Optionally produce graphical output showing 3D distances for each 2D point.
- random_patch_path.py : produce a large number of random overlapping patch sequences of the specified length 
- show_patchpositions.py : tkinter program to display a patch path produced by random_patch_path.py. Mainly used to check that random_patch_path.py is behaving correctly. Sometimes useful to help visualise and understand outputs.
- combine_badscore.py : if find_bad_patches.py has been run separetely from several threads, use this program to combine the outputs.
- print_badscore.py : displays the resulting bad patches.
- patchnum_from_rgb.py : from the rgb value produced by zarr_show2_64.cpp, identify the patch number
