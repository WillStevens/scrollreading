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


# Generate parameters.h and parameters.py from parameters.json
python parse_parameters.py

# Programs that make up the main part of the pipeline
g++ -msse4.1 -O3 -Wall simpaper9.cpp -o simpaper9 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall interpolate.cpp -o interpolate
g++ -O3 -Wall addtobigpatch.cpp -o addtobigpatch -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall align_patches6.cpp -o align_patches6 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall flip_patch2.cpp -o flip_patch2
g++ -O3 -Wall erasepoints.cpp -o erasepoints -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall randombigpatchpoint.cpp -o randombigpatchpoint -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall find_nearest4.cpp -o find_nearest4 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall listbigpatchpoints.cpp -o listbigpatchpoints -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall listpatchpoints.cpp -o listpatchpoints -lblosc2 -L/usr/local/lib -I/usr/local/include

# Program for showing output
g++ -O3 -Wall render_from_zarr5.cpp -o render_from_zarr5 -lblosc2 -ltiff -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall zarr_show2_64.cpp -o zarr_show2_64 -lblosc2 -ltiff -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall zarr_show2_u8.c -o zarr_show2_u8 -lblosc2 -ltiff -L/usr/local/lib -I/usr/local/include

# Programs for downloading zarr data
#g++ -O3 -Wall buffer_zarr.c -o buffer_zarr -lblosc2 -lcurl -ljson-c -L/usr/local/lib -I/usr/local/include

# Programs for processing zarr data
g++ -O3 -Wall papervectorfield_closest3.c -o papervectorfield_closest3 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall vectorfield_smooth_64.c -o vectorfield_smooth_64 -lblosc2 -L/usr/local/lib -I/usr/local/include

# Programs for finding bad patches
g++ -O3 -Wall find_mismatch.cpp -o find_mismatch -lblosc2 -L/usr/local/lib -I/usr/local/include
