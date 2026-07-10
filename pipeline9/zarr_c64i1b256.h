#pragma once

#include <blosc2.h>
typedef int8_t ZARRType_c64i1b256;

typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[sizeof(ZARRType_c64i1b256)*1048576+BLOSC2_MAX_OVERHEAD];
    ZARRType_c64i1b256 buffers[256][64][64][64][4];
    int bufferIndex[256][4];
    unsigned char written[256];
    uint64_t bufferUsed[256];
    ZARRType_c64i1b256 (*buffer)[64][64][64][4];

    int index;
  
    uint64_t counter;
} ZARR_c64i1b256;

ZARR_c64i1b256 *ZARROpen_c64i1b256(const char *location);
int ZARRFlushOne_c64i1b256(ZARR_c64i1b256 *z, int i);
int ZARRFlush_c64i1b256(ZARR_c64i1b256 *z);
int ZARRClose_c64i1b256(ZARR_c64i1b256 *z);
/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_c64i1b256(ZARR_c64i1b256 *z, int c[4]);
ZARRType_c64i1b256 ZARRRead_c64i1b256(ZARR_c64i1b256 *za,int x0,int x1,int x2,int x3);
// Read several values from the last dimensions
void ZARRReadN_c64i1b256(ZARR_c64i1b256 *za,int x0,int x1,int x2,int x3,int n,ZARRType_c64i1b256 *v);
int ZARRWrite_c64i1b256(ZARR_c64i1b256 *za,int x0,int x1,int x2,int x3,ZARRType_c64i1b256 value);
void ZARRWriteN_c64i1b256(ZARR_c64i1b256 *za,int x0,int x1,int x2,int x3,int n, ZARRType_c64i1b256 *v);
// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN_c64i1b256(ZARR_c64i1b256 *za,int x0,int x1,int x2,int x3,int n, ZARRType_c64i1b256 *v);
