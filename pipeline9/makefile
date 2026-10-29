CC=g++
CFLAGS=-O3 -Wall -fopenmp 
#CFLAGS=-pg -O2 -g -Wall -fopenmp 
LFLAGS=-lblosc -lblosc2 -ltiff -L/usr/lib -I/usr/include

simpaper10_tmp: common_types.o patch_generator.o simpaper10.o zarr_1.o zarr_1_b700.o bigpatch.o align_patches.o erasepoints.o badpatchfinder.o zarr_show2_u8.o sliceanimrender.o position_patches.o calcvectorfield.o patchcolourkey.o PatchSpringSimulation.o anneal.o visitorder.o scoreplacement.o omissiontest.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

PatchSpringSimulation.o: PatchSpringSimulation.cpp PatchSpringSimulation.hpp
	$(CC) -c $< $(CFLAGS)

anneal.o: anneal.cpp anneal.h visitorder.h scoreplacement.h PatchSpringSimulation.cpp
	$(CC) -c $< $(CFLAGS)

omissiontest.o: omissiontest.cpp omissiontest.h visitorder.h scoreplacement.h PatchSpringSimulation.cpp
	$(CC) -c $< $(CFLAGS)

visitorder.o: visitorder.cpp visitorder.h position_patches.h
	$(CC) -c $< $(CFLAGS)

scoreplacement.o: scoreplacement.cpp scoreplacement.h
	$(CC) -c $< $(CFLAGS)
	
zarr_1.o: zarr_1.c zarr_1.h
	$(CC) -c $< $(CFLAGS)

zarr_1_b700.o: zarr_1_b700.c zarr_1_b700.h
	$(CC) -c $< $(CFLAGS)

zarr_show2_u8.o : zarr_show2_u8.cpp zarr_show2_u8.h zarr_1_b700.h parameters.h common_types.h
	$(CC) -c $< $(CFLAGS)

calcvectorfield.o : calcvectorfield.cpp parameters.h vec3.h zarr_1.h
	$(CC) -c $< $(CFLAGS)

position_patches.o : position_patches.cpp position_patches.h parameters.h common_types.h
	$(CC) -c $< $(CFLAGS)
	
sliceanimrender.o : sliceanimrender.cpp sliceanimrender.h zarr_show2_u8.h parameters.h common_types.h
	$(CC) -c $< $(CFLAGS)

patchcolourkey.o : patchcolourkey.cpp patchcolourkey.h
	$(CC) -c $< $(CFLAGS)
	
bigpatch.o: bigpatch.cpp bigpatch.h common_types.h
	$(CC) -c $< $(CFLAGS)

common_types.o: common_types.cpp common_types.h vec3.h
	$(CC) -c $< $(CFLAGS)

patch_generator.o: patch_generator.cpp patch_generator.h parameters.h common_types.h vec3.h zarr_1.h calcvectorfield.h
	$(CC) -c $< $(CFLAGS)

simpaper10.o: simpaper10.cpp badpatchfinder.h erasepoints.h align_patches.h patch_generator.h sliceanimrender.h patchcolourkey.h bigpatch.h common_types.h vec3.h parameters.h PatchSpringSimulation.hpp visitorder.h anneal.h omissiontest.h
	$(CC) -c $< $(CFLAGS)
	
align_patches.o : align_patches.cpp align_patches.h bigpatch.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)

erasepoints.o : erasepoints.cpp erasepoints.h bigpatch.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)
	
badpatchfinder.o : badpatchfinder.cpp badpatchfinder.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)

parameters.h : parameters.json
	python parse_parameters.py


render_from_zarr6: render_from_zarr6.o bigpatch.o common_types.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
render_from_zarr6.o: render_from_zarr6.cpp bigpatch.h zarr_4.c
	$(CC) -c $< $(CFLAGS)

render_from_zarr6_uint16: render_from_zarr6_uint16.o bigpatch.o common_types.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
render_from_zarr6_uint16.o: render_from_zarr6_uint16.cpp bigpatch.h zarr_4.c
	$(CC) -c $< $(CFLAGS)

make_surface_vol.o: make_surface_vol.cpp zarr_svol.c zarr_4_b.c
	$(CC) -c $< $(CFLAGS)

make_surface_vol: make_surface_vol.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

zarrconv.o: zarrconv.c
	$(CC) -c $< $(CFLAGS)

zarrconv: zarrconv.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
vectorfieldtest: vectorfieldtest.o calcvectorfield.o zarr_1.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

vectorfieldtest.o: vectorfieldtest.cpp calcvectorfield.h zarr_c64i1b256.h
	$(CC) -c $< $(CFLAGS)
