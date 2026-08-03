This pipeline (pipeline9) is derived from earlier pipelines. The parameters.json file is set up for PHerc0139

The main differences between this and previous version of the pipeline are:
- A vector field zarr is no longer needed, the vector field needed by the patch generator is calculated at run-time and cached in memory. 
- The whole pipeline is now a single C++ program. This is so that operations can be carried out in-memory with having to save and load to disk.
  (This is only partly realized - the only part that runs fully in-memory is the simulated annealing step). 

This is a C++ makefile project. It is a low-dependency quick-build project. blosc2 and libtiff-dev are the only C++ dependencies. It typically takes a few seconds to compile and link.
You will need at least 16Gb RAM to generate and process patches for a whole scroll (e.g. scroll 4). Larger scrolls probably need 32Gb.

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
11. To output a list of 3D coordinates for the central 2D point of each patch run './simpaper10 l'
12. To produce a visit order and initial 2D placement of patches run './simpaper10 v'
13. To refine positions using a ball-and-spring model run './simpaper h'
14. To flatten the resulting surface run './simpaper f 30'. This will produce patch_0.bin in OUTPUT_DIR.
    patch_0.bin is a binary file of ux,uy,vx,vy,vz. It can be converted to CSV using bin2csv.
    The resulting csv file can be converted to tifxyz using csv2tifxyz
    

The pipeline has been run on windows (using cygwin) and on linux.

This is the complete list of one-character commands and paramegeters that the program accepts, showing which input files they need and which outputs they produce. Read report12.pdf for a high level overview of what the program does.

g [n] : generate n patches. Requires folders surface.bp/surface, boundary.bp/surface and patches to exist in the output folder. Produces .bin patches in the 'patches' folder. The surface.bp and boundary.bp folders keep track of the growing surface and its boundary during patch growth. The file 'rel.csv' stores relationships between overlapping patches.

r [n] : restart generation for a further n patches. Requires the same folders as above, and produces the same outputs.

b : Obsolete function for finding bad patches. Use c instead.

c : Reads patches from patches folder and outputs badpatches.csv (listing bad patches) and badpatchscores.csv (showing how they score).

l : produce 3D x,y,z coords for each patch centre and write to patchVolCoords.csv

v : Inputs badpatches.csv and optionally manualBadPatch.csv and produces a visit order. Write it to visitorder.csv and in alignmentorder.txt write 2D positions. Also outputs badbridges_out.csv which lists patches that connect other patches with implausibly different 2D coords.

h : inputs patchVolCoords and alignmentorder.txt and outputs patchPositions.txt : balances 2D positions using a ball and spring model.

f [d] : reads patchorder.csv and patchPositions.txt and outputs a single flatten patch made from all of the smaller patches. Writes the output to patch_0.bin in the output folder.

a : produce an animation showing growing patches. Reads patchorder.csv and pathchPositions.txt. Needs sliceanim folder for output and tifbus for cache.

A : as above, but colours patches using global 2D coords - useful for spotting sheet switching

p [patchnum...] : like a, but for specified patches rather than all patches.

q patchnum x y : gives the 3D coord of local 2D coord of a patch (where 0 0 = patch centre)

z zcoord : show z-slice through scroll, showing all patches in that slice.

n : using badpatches.csv, manualBadPatch.csv (optional) and annealState.csv (optional), run simulated annealing. Output is annealState_out.csv and annealStats.csv. Copy annealState_out.csv to annealState.csv if rerunning again from previous state. Copy annealState_out.csv to manualBadPatch.csv to use results of annealing in other commands. (i.e. to exclude patches found by annealing)



