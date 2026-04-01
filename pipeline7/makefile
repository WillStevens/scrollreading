CC=g++
CFLAGS=-O3 -Wall
LFLAGS=-lblosc2 -ltiff -L/usr/local/lib -I/usr/local/include

simpaper10: common_types.o patch_generator.o simpaper10.o zarr_1.o zarr_c64i1b256.o bigpatch.o align_patches.o erasepoints.o badpatchfinder.o zarr_show2_u8.o sliceanimrender.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
zarr_1.o: zarr_1.c zarr_1.h
	$(CC) -c $< $(CFLAGS)

zarr_c64i1b256.o: zarr_c64i1b256.c zarr_c64i1b256.h
	$(CC) -c $< $(CFLAGS)

zarr_show2_u8.o : zarr_show2_u8.cpp zarr_show2_u8.h parameters.h common_types.h
	$(CC) -c $< $(CFLAGS)

sliceanimrender.o : sliceanimrender.cpp sliceanimrender.h zarr_show2_u8.h parameters.h common_types.h
	$(CC) -c $< $(CFLAGS)

bigpatch.o: bigpatch.cpp bigpatch.h
	$(CC) -c $< $(CFLAGS)

common_types.o: common_types.cpp common_types.h vec3.h
	$(CC) -c $< $(CFLAGS)

patch_generator.o: patch_generator.cpp patch_generator.h parameters.h common_types.h vec3.h zarr_c64i1b256.h zarr_1.h
	$(CC) -c $< $(CFLAGS)

simpaper10.o: simpaper10.cpp badpatchfinder.h erasepoints.h align_patches.h patch_generator.h bigpatch.h common_types.h vec3.h parameters.h
	$(CC) -c $< $(CFLAGS)
	
align_patches.o : align_patches.cpp align_patches.h bigpatch.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)

erasepoints.o : erasepoints.cpp erasepoints.h bigpatch.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)
	
badpatchfinder.o : badpatchfinder.cpp badpatchfinder.h common_types.h parameters.h

parameters.h : parameters.json
	python parse_parameters.py

