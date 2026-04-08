#pragma once
#include <blosc2.h>
typedef uint8_t ZARRType_1_b700;

typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[sizeof(ZARRType_1_b700)*2097152+BLOSC2_MAX_OVERHEAD];
    ZARRType_1_b700 buffers[800][128][128][128];
    int bufferIndex[800][3];
    unsigned char written[800];
    uint64_t bufferUsed[800];
    ZARRType_1_b700 (*buffer)[128][128][128];

    int index;
  
    uint64_t counter;
} ZARR_1_b700;

ZARR_1_b700 *ZARROpen_1_b700(const char *location);
int ZARRFlushOne_1_b700(ZARR_1_b700 *z, int i);
int ZARRFlush_1_b700(ZARR_1_b700 *z);
int ZARRClose_1_b700(ZARR_1_b700 *z);
/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_1_b700(ZARR_1_b700 *z, int c[3]);
ZARRType_1_b700 ZARRRead_1_b700(ZARR_1_b700 *za,int x0,int x1,int x2);
// Read several values from the last dimensions
void ZARRReadN_1_b700(ZARR_1_b700 *za,int x0,int x1,int x2,int n,ZARRType_1_b700 *v);
int ZARRWrite_1_b700(ZARR_1_b700 *za,int x0,int x1,int x2,ZARRType_1_b700 value);
void ZARRWriteN_1_b700(ZARR_1_b700 *za,int x0,int x1,int x2,int n, ZARRType_1_b700 *v);
// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN_1_b700(ZARR_1_b700 *za,int x0,int x1,int x2,int n, ZARRType_1_b700 *v);
