This pipeline (pipeline9) is derived from earlier pipelines. The parameters.json file is set up for PHerc0139

The main differences between this and previous version of the pipeline are:
- A vector field zarr is no longer needed, the vector field needed by the patch generator is calculated at run-time and cached in memory. 
- The whole pipeline is now a single C++ program. This is so that operations can be carried out in-memory with having to save and load to disk.
  (This is only partly realized - the only part that runs fully in-memory is the simulated annealing step). 

This is a C++ makefile project.

To download and run this pipeline, follow these steps:

1. Install the blosc2 and libtiff-dev C libraries.
2. Download a surface prediction OME zarr (only the full resolution 0 subfolder is needed)
3. Download a volume OME zarr (only the quarter resolution 2 subfolder of the OME zarr is essential)
4. Do a sparse git clone to get just this pipeline9 folder:
   ```
   git clone --no-checkout https://github.com/WillStevens/scrollreading.git
   cd scrollreading
   git sparse-checkout init --no-cone
   git sparse-checkout set pipeline9
   git checkout @
   ```
5. Set directory locations for the two Zarrs obtained above in parameters.json: "VOLUME_ZARR" and "SURFACE_ZARR"
6. Set the output directory location (all patches and other files that the pipeline produces will be placed here), e.g.: `"OUTPUT_DIR" : "d:/pipelineOutput"`
7. Create the output directory, and within it make the folders 'surface.bp/surface' and 'boundary.bp/surface' and 'patches'
7. Run `make'. This should only take a few seconds.
8. The pipeline is now ready to run. Generate some patches using: `./simpaper10 g 100`
9. The pipeline will have produced:
    - surface.bp - a chunked compressed data structure containing the patches at full resolution.
    - boundary.bp - a representation of the boundary used by the pipeline to work out the next seed point.
    - patches/*.bin - all of the patches in a binary qx,qy,vx,vy,vz format (q=quadmesh, v=volume). These are in low resolution (4-voxels per quadmesh point).
    - rel.csv - this contains the relationships between all overlapping patches, represented as an affine transformation, along with the variance of the transformations sampled.
10. To find problem patches run './simpaper10 c'. This will load the patches already produced, look for problems, and output badpatches.csv which lists all of the problem patches.
11. To produce a visit order and initial 2D placement of patches run './simpaper10 v'
12. To refine positions using a ball-and-spring model run './simpaper h'
13. To flatten the resulting surface run './simpaper f 30'. This will produce patch_0.bin in OUTPUT_DIR.
    patch_0.bin is a binary file of ux,uy,vx,vy,vz. It can be converted to CSV using bin2csv.
    The resulting csv file can be converted to tifxyz using csv2tifxyz
    

The pipeline has been run on windows (using cygwin) and on linux.


