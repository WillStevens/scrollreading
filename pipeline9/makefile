CC=g++
# Portability: on many modern distributions 'python' does not exist, only 'python3' does.
# It can be forced with  make PYTHON=/path/to/python
PYTHON ?= $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null)
# -ffp-contract=off is mandatory, not a precaution: 'fast' is the GCC default and would enable
# contraction into FMA as soon as the target has the instruction, changing the rounding and
# therefore the results, with nothing anywhere declaring that it happened.
#
# NATIVE=1 compiles for the machine it is built on (faster, not a portable binary). The default
# stays portable. Never use -Ofast or -ffast-math: they change the results.
NATIVE ?= 0
ifeq ($(NATIVE),1)
  ARCHFLAGS = -march=native -mtune=native
endif
# Where blosc2 and libtiff are installed. The defaults are the /usr/local the original makefile
# searched, so a plain 'make' builds as before against the system libraries. Libraries installed
# elsewhere are named on the command line:
#   make BLOSC2_PREFIX=/opt/blosc2 LIBTIFF_PREFIX=/opt/libtiff
# RPATH=1 records both lib folders in the binaries, so that they run without LD_LIBRARY_PATH
# when those folders are not on the loader's default path. Off by default: a binary built
# against the system libraries should carry no path.
# Two optional build variables, both off by default, neither of which changes a computed value:
#   LTO=1           link time optimisation (-flto=auto).
#   PGO=generate    an instrumented build: running it writes a profile (*.gcda) next to the objects.
#   PGO=use         a build that reads that profile. The profile comes from one training run of the
#                   PGO=generate binary on real data; delete the objects and binaries between the two
#                   builds, and keep the .gcda files.
# -fprofile-update=atomic because the growth is threaded; -fprofile-partial-training keeps the
# code the training run never reached optimised as usual. Combined with NATIVE=1 (or
# ARCHFLAGS=-march=native) the binary runs only on CPUs with the builder's instruction sets.
LTO ?= 0
ifeq ($(LTO),1)
  OPTFLAGS += -flto=auto
endif
ifeq ($(PGO),generate)
  OPTFLAGS += -fprofile-generate -fprofile-update=atomic
endif
ifeq ($(PGO),use)
  OPTFLAGS += -fprofile-use -fprofile-partial-training -Wno-error=coverage-mismatch
endif
BLOSC2_PREFIX ?= /usr/local
LIBTIFF_PREFIX ?= /usr/local
RPATH ?= 0
ifeq ($(RPATH),1)
  RPATHFLAGS = -Wl,-rpath,$(BLOSC2_PREFIX)/lib -Wl,-rpath,$(LIBTIFF_PREFIX)/lib
endif
CFLAGS=-O3 -w -fopenmp -ffp-contract=off $(ARCHFLAGS) $(OPTFLAGS) -I$(BLOSC2_PREFIX)/include -I$(LIBTIFF_PREFIX)/include
LFLAGS=-lblosc2 -ltiff -L$(BLOSC2_PREFIX)/lib -L$(LIBTIFF_PREFIX)/lib $(RPATHFLAGS)

# parameters.h is generated from parameters.json and is included by most units, but three of
# the rules below do not list it: anneal.o, visitorder.o and omissiontest.o. Change
# parameters.json, run make, and those three keep the previous OUTPUT_DIR compiled in, so
# patchorder.csv and annealState_out.csv are written to the previous run's folder while the
# rest of the pipeline uses the new one, with no error anywhere.
$(patsubst %.cpp,%.o,$(wildcard *.cpp)) $(patsubst %.c,%.o,$(wildcard *.c)): parameters.h

simpaper10: seed_runtime.o common_types.o patch_generator.o simpaper10.o zarr_1.o zarr_1_b700.o bigpatch.o align_patches.o erasepoints.o badpatchfinder.o zarr_show2_u8.o sliceanimrender.o position_patches.o calcvectorfield.o patchcolourkey.o PatchSpringSimulation.o anneal.o visitorder.o scoreplacement.o omissiontest.o
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
	
align_patches.o : align_patches.cpp align_patches.h cellgrid.h bigpatch.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)

erasepoints.o : erasepoints.cpp erasepoints.h cellgrid.h bigpatch.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)
	

badpatchfinder.o : badpatchfinder.cpp badpatchfinder.h common_types.h parameters.h
	$(CC) -c $< $(CFLAGS)

parameters.h : parameters.json
	$(PYTHON) parse_parameters.py


render_from_zarr6: render_from_zarr6.o bigpatch.o common_types.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
render_from_zarr6.o: render_from_zarr6.cpp bigpatch.h zarr_4.c
	$(CC) -c $< $(CFLAGS)

render_from_zarr6_uint16: render_from_zarr6_uint16.o bigpatch.o common_types.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)
	
render_from_zarr6_uint16.o: render_from_zarr6_uint16.cpp bigpatch.h zarr_4.c
	$(CC) -c $< $(CFLAGS)
	
vectorfieldtest: vectorfieldtest.o calcvectorfield.o zarr_1.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

vectorfieldtest.o: vectorfieldtest.cpp calcvectorfield.h zarr_c64i1b256.h
	$(CC) -c $< $(CFLAGS)

# bin2tifxyz had no rule: make's implicit rule does not link libtiff (rule added 2026-09-12)
bin2tifxyz: bin2tifxyz.cpp
	$(CXX) $(CFLAGS) -o bin2tifxyz bin2tifxyz.cpp $(LFLAGS)

# the bit test of the AVX-512 kernels against the scalar code of the same build (needs AVX-512F to run)
kernel_test: kernel_test.cpp patch_generator.o calcvectorfield.o common_types.o zarr_1.o zarr_1_b700.o
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

# a copy of the surface prediction in LZ4HC, checked chunk by chunk (see the head of recode_zarr_lz4hc.c)
recode_zarr_lz4hc: recode_zarr_lz4hc.c
	$(CC) -o $@ $< $(CFLAGS) $(LFLAGS)

# the seed point and the surface zarr read at start, so one binary serves every seed (seed_runtime.cpp)
seed_runtime.o: seed_runtime.cpp parameters.h
	$(CC) -c $< $(CFLAGS)
