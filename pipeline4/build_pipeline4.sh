#!/bin/sh
g++ -msse4.1 -O3 -Wall simpaper8.cpp -o simpaper8 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall interpolate.cpp -o interpolate
g++ -O3 -Wall addtobigpatch.cpp -o addtobigpatch -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall align_patches6.cpp -o align_patches6 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall flip_patch2.cpp -o flip_patch2
g++ -O3 -Wall erasepoints.cpp -o erasepoints -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall randombigpatchpoint.cpp -o randombigpatchpoint -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall find_nearest4.cpp -o find_nearest4 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall listbigpatchpoints.cpp -o listbigpatchpoints -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall listpatchpoints.cpp -o listpatchpoints -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall render_from_zarr5.cpp -o render_from_zarr5 -lblosc2 -ltiff -L/usr/local/lib -I/usr/local/include

g++ -O3 -Wall buffer_zarr.c -o buffer_zarr -lblosc2 -lcurl -ljson-c -L/usr/local/lib -I/usr/local/include
