CC=g++
CFLAGS=-O3 -Wall
LFLAGS=-lblosc2 -L/usr/local/lib -I/usr/local/include

simpaper10: common_types.o patch_generator.o simpaper10.o zarr_1.o zarr_c64i1b256.o bigpatch.o align_patches.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
zarr_1.o: zarr_1.c zarr_1.h
	$(CC) -c $< $(CFLAGS)

zarr_c64i1b256.o: zarr_c64i1b256.c zarr_c64i1b256.h
	$(CC) -c $< $(CFLAGS)

bigpatch.o: bigpatch.cpp bigpatch.h
	$(CC) -c $< $(CFLAGS)

common_types.o: common_types.cpp common_types.h vec3.h
	$(CC) -c $< $(CFLAGS)

patch_generator.o: patch_generator.cpp patch_generator.h common_types.h vec3.h zarr_c64i1b256.h zarr_1.h
	$(CC) -c $< $(CFLAGS)

simpaper10.o: simpaper10.cpp patch_generator.h common_types.h vec3.h parameters.h
	$(CC) -c $< $(CFLAGS)
	
align_patches.o : align_patches.cpp common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)
	
parameters.h : parameters.json
	python parse_parameters.py

