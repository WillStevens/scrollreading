g++ -msse4.1 -O3 -Wall simpaper8.cpp -o simpaper8 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall interpolate.cpp -o interpolate
g++ -O3 -Wall addtobigpatch.cpp -o addtobigpatch -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall align_patches5.cpp -o align_patches5 -lblosc2 -L/usr/local/lib -I/usr/local/include
g++ -O3 -Wall flip_patch2.cpp -o flip_patch2
g++ -O3 -Wall erasepoints.cpp -o erasepoints -lblosc2 -L/usr/local/lib -I/usr/local/include
