#pragma once
#include <blosc2.h>
typedef uint8_t ZARRType_1;

typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[sizeof(ZARRType_1)*7077888+BLOSC2_MAX_OVERHEAD];
    ZARRType_1 buffers[80][192][192][192];
    int bufferIndex[80][3];
    unsigned char written[80];
    uint64_t bufferUsed[80];
    ZARRType_1 (*buffer)[192][192][192];

    int index;
  
    uint64_t counter;
} ZARR_1;

ZARR_1 *ZARROpen_1(const char *location);
int ZARRFlushOne_1(ZARR_1 *z, int i);
int ZARRFlush_1(ZARR_1 *z);
int ZARRClose_1(ZARR_1 *z);
/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_1(ZARR_1 *z, int c[3]);
ZARRType_1 ZARRRead_1(ZARR_1 *za,int x0,int x1,int x2);
// Read several values from the last dimensions
void ZARRReadN_1(ZARR_1 *za,int x0,int x1,int x2,int n,ZARRType_1 *v);
int ZARRWrite_1(ZARR_1 *za,int x0,int x1,int x2,ZARRType_1 value);
void ZARRWriteN_1(ZARR_1 *za,int x0,int x1,int x2,int n, ZARRType_1 *v);
// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN_1(ZARR_1 *za,int x0,int x1,int x2,int n, ZARRType_1 *v);
