#!/bin/sh

# When running on linux may need to tell it where to find blosc2 shared library
#export LD_LIBRARY_PATH=$LB_LIBRARY_PATH:/usr/local/lib64

# This script builds all of the C/C++ programs needed to run the pipeline

# e.g. to build and run the pipeline on a new EC2 instances:
# Install blosc2 libtiff-dev
# git clone ??
# <set the paths in the parameters.json file>
# cd ??
# ./build_pipeline5.sh
# python sp_pipeline4.py 100

# You should now find 'surface.tif' with an image of the surface

# Make the various ZARR reader/writers that we need
# 80 buffers for zarr_1 because this is used for rendering, and we want to fit a whole row/column of a scroll in memory
python zarrgen.py zarr_meta_1.json 80 _1 > zarr_1.c 
python zarrgen.py zarr_meta_c128i1.json 8 _c128i1b8 > zarr_c128i1b8.c
python zarrgen.py zarr_meta_c128i1.json 16 _c128i1b16 > zarr_c128i1b16.c
python zarrgen.py zarr_meta_c128i1.json 128 _c128i1b128 > zarr_c128i1b128.c
python zarrgen.py zarr_meta_c64i1.json 1 _c64i1b1 > zarr_c64i1b1.c
python zarrgen.py zarr_meta_c64i1.json 64 _c64i1b64 > zarr_c64i1b64.c
python zarrgen.py zarr_meta_c64i1.json 256 _c64i1b256 > zarr_c64i1b256.c
python zarrgen.py zarr_meta_c128.json 80 _c128 > zarr_c128.c 


# Generate parameters.h and parameters.py from parameters.json
python parse_parameters.py

# Programs that make up the main part of the pipeline
g++ -O3 -Wall -c common_types.cpp
g++ -O3 -Wall -c simpaper10.cpp
g++ simpaper10.o common_types.o -o simpaper10 -lblosc2 -L/usr/local/lib -I/usr/local/include
