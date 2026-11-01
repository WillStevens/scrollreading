This pipeline (pipeline9) is derived from earlier pipelines. The parameters.json file is set up for PHerc1667

Note that PHerc1667 uses some slightly different zarr formats than prize-eligible scrolls, so when using with prize-eligible scrolls you need to swap zarr_1.c/.h and zarr_4.c/.h with the appropriate uint16/uint8 and chunk size 128/192 versions of these.

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
11. To output a list of 3D coordinates for the central 2D point of each patch run './simpaper10 l' (PatchSpringSimulation.cpp needs this - i.e. the 'simpaper10 h' command)
12. To produce a visit order and initial 2D placement of patches run './simpaper10 v'
13. To refine positions using a ball-and-spring model run './simpaper h'
14. To flatten the resulting surface run './simpaper f 30'. This will produce a segment called patch_0.bin in OUTPUT_DIR. (This output is called patch_0.bin rather than segment_0.bin simply because I haven't got around to updating the Patch class to use another filename prefix when outputting segments).
    patch_0.bin is a binary file of ux,uy,vx,vy,vz. It can be converted to CSV using bin2csv. It can be converted to a tifxyz file using bin2tifxyz. Note that the scale in the tifxyz meta.json file will be written incorrectly as 1,1 - you need to manually edit it to 0.25,0.25
    

The pipeline has been run on windows (using cygwin) and on linux.

This is the complete list of one-character commands and parameters that the program accepts, showing which input files they need and which outputs they produce. Read report12.pdf for a high level overview of what the program does.

Not that when there is a command such as v,n,f and also a vm,nm,fm command, the -m versions handle many segments at the same time. These ones have been under active development recently, whereas the v,n,f versions have not, so might not have some of the features that vm,nm,fm have.

g [n] : generate n patches. Requires folders surface.bp/surface, boundary.bp/surface and patches to exist in the output folder. Produces .bin patches in the 'patches' folder. The surface.bp and boundary.bp folders keep track of the growing surface and its boundary during patch growth. The file 'rel.csv' stores relationships between overlapping patches. You must empty the surface.bp/surface and boundary.bp/boundary and patches folder before running this.

r [n] : restart generation for a further n patches. Requires the same folders as above, and produces the same outputs.

b : Obsolete function for finding bad patches. Use c instead.

pa : Produce patch alignment scores. These are needed by the v/vm commands. Patch alignment score for patch A is based on average alignment of A with all of its neighbours.

c : Reads patches from patches folder and outputs badpatches.csv (listing bad patches) and badpatchscores.csv (showing how they score).

l : produce 3D x,y,z coords for each patch centre and write to patchVolCoords.csv

v : Inputs badpatches.csv and optionally manualBadPatch.csv and produces a visit order. Write it to visitorder.csv and in alignmentorder.txt write 2D positions. Also outputs badbridges_out.csv which lists patches that connect other patches with implausibly different 2D coords.

vm number-of-segments : Like v, but produces visit orders for the specified number of segments. 
vmf number-of-segments : A debugging command that does what vm does, but also checks whether the 2 largest components have patches that align and shows what the flip state is for the alignment. (Written to check that the two largest segments of PHerc1667 were flipped with respect to each other).

h : inputs patchVolCoords and alignmentorder.txt and outputs patchPositions.txt : balances 2D positions using a ball and spring model.

hm number-of-segments : Like h, but works with multiple segments at once.

f d : reads patchorder.csv and patchPositions.txt and outputs a single flatten patch made from all of the smaller patches. Writes the output to patch_0.bin in the output folder. d is a 3D space normal distance at which two patches are definitely considered to be inconsistent - i.e. if two points that have the same 2D coords have a 3D normal distance >= d then this is considered wrong. 30 is often used, but is probably high. It affects how darkly red points are coloured, and also affects what it reported on human-readable output, so it is a somewhat cosmetic parameter. 

fm d number-of-segments : Like f, but works with multiple segments at once. 

a : produce an animation showing growing patches. Reads patchorder.csv and pathchPositions.txt. Needs sliceanim folder for output and tifbus for cache.

A : as above, but colours patches using global 2D coords - useful for spotting sheet switching

p [patchnum...] : like a, but for specified patches rather than all patches.

q patchnum x y : gives the 3D coord of local 2D coord of a patch (where 0 0 = patch centre)

x patchnum0 patchnum1 <flip 0 or 1> seed : examine the alignment of two patches, either unflipped or flipped. A random seed is needed because the alignment algorithm selects random overlapping points.

s zcoord : show z-slice through scroll, showing all patches in that slice.

n iterations initT0 : using badpatches.csv, manualBadPatch.csv (optional) and annealState.csv (optional), run simulated annealing. Output is annealState_out.csv and annealStats.csv. Copy annealState_out.csv to annealState.csv if rerunning again from previous state. Copy annealState_out.csv to manualBadPatch.csv to use results of annealing in other commands. (i.e. to exclude patches found by annealing). 100 iterations should take a few minutes. 100-1000 is a good value for initT0.

nm number-of-segments iterations initT0 : Like n, but works on multiple segments at once.

examine_anneal.sh is used to render results of annealing. You will need to edit paths manually in this script - it doesn't get them from parameters.json.

There are numerous small standalone Claude-written (or human-written) utility programs in the folder that aren't built by make and have to be manually built when you need them. Claude usually puts the compilation instructions in a comment at the top of the source file.

n
