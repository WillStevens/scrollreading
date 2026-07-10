
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <blosc2.h>
typedef int8_t ZARRType_c128i1b8;

typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[sizeof(ZARRType_c128i1b8)*8388608+BLOSC2_MAX_OVERHEAD];
    ZARRType_c128i1b8 buffers[8][128][128][128][4];
    int bufferIndex[8][4];
    unsigned char written[8];
    uint64_t bufferUsed[8];
    ZARRType_c128i1b8 (*buffer)[128][128][128][4];

    int index;
  
    uint64_t counter;
} ZARR_c128i1b8;

ZARR_c128i1b8 *ZARROpen_c128i1b8(const char *location)
{
	ZARR_c128i1b8 *z = (ZARR_c128i1b8 *)malloc(sizeof(ZARR_c128i1b8));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<8; i++)
	{
	  z->written[i] = 0;
      for(int j = 0; j<4; j++)
        z->bufferIndex[i][j] = -1;
	}
	
    strcpy(z->location,location);
	
	z->counter = 1;

    for(int i = 0; i<8; i++)
      z->bufferUsed[i] = 0;
  
	return z;
}

int ZARRFlushOne_c128i1b8(ZARR_c128i1b8 *z, int i)
{
    if (z->written[i])
	{
      sprintf(z->location+z->locationRootLength,"/%d.%d.%d.%d",z->bufferIndex[i][0],z->bufferIndex[i][1],z->bufferIndex[i][2],z->bufferIndex[i][3]);

      blosc1_set_compressor("zstd");
	  int compressed_len = blosc2_compress(3,1,sizeof(ZARRType_c128i1b8),z->buffers[i],sizeof(ZARRType_c128i1b8)*8388608,z->compressedData,sizeof(ZARRType_c128i1b8)*8388608+BLOSC2_MAX_OVERHEAD);

      if (compressed_len <= 0) {
        return -1;
      }

	  FILE *f = fopen(z->location,"wb");
	  fwrite(z->compressedData,1,compressed_len,f);
	  fclose(f);
	
	  z->written[i] = 0;
	}
	
	return 0;
}

int ZARRFlush_c128i1b8(ZARR_c128i1b8 *z)
{
	for(int i = 0; i<8; i++)
	{
		if (z->bufferIndex[i][0] != -1)
		{
			ZARRFlushOne_c128i1b8(z,i);
		}
	}
	
	return 0;
}

int ZARRClose_c128i1b8(ZARR_c128i1b8 *z)
{
    ZARRFlush_c128i1b8(z);
    free(z->location);
    free(z);
	
	return 0;
}

/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_c128i1b8(ZARR_c128i1b8 *z, int c[4])
{	
	if (z->buffer && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2] && c[3] == z->bufferIndex[z->index][3])
		return 0;

	for(z->index = 0; z->index < 8; z->index++)
	{
		if (1  && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2] && c[3] == z->bufferIndex[z->index][3])
		{
			z->buffer = &z->buffers[z->index];
			z->bufferUsed[z->index] = z->counter++;
			return 0;
		}
	}
		
	for(z->index = 0; z->index < 8; z->index++)
	{
		if (z->bufferIndex[z->index][0]==-1)
			break;
	}
  	
	if (z->index == 8)
	{
		/* Find the buffer that was least recently used and free it up */
		printf("Ran out of buffers - flushing oldest\n");
		int oldestIndex = 0;
		uint64_t oldestAge = z->bufferUsed[0];
		for(int i = 1; i<8; i++)
		{
			if (z->bufferUsed[i]<oldestAge)
			{
				oldestAge = z->bufferUsed[i];
				oldestIndex = i;
			}
		}
		
		ZARRFlushOne_c128i1b8(z,oldestIndex);
		z->index = oldestIndex;
	}
    z->bufferIndex[z->index][0] = c[0];
    z->bufferIndex[z->index][1] = c[1];
    z->bufferIndex[z->index][2] = c[2];
    z->bufferIndex[z->index][3] = c[3];

	z->buffer = &z->buffers[z->index];
	z->bufferUsed[z->index] = z->counter++;
	z->written[z->index] = 0;
    sprintf(z->location+z->locationRootLength,"/%d.%d.%d.%d",z->bufferIndex[z->index][0],z->bufferIndex[z->index][1],z->bufferIndex[z->index][2],z->bufferIndex[z->index][3]);

    FILE *f = fopen(z->location,"rb");

    //printf("Opening:%s\n",z->location);	
	if (!f)
	{
		printf("Did not find file:%s\n",z->location); // Useful to display this message because it often indicates a file naming problem

		memset(z->buffer,0,sizeof(ZARRType_c128i1b8)*8388608);

		//No need to count it as written to yet - if it remains empty then just leave the file as non-existent
	    //z->written[z->index] = 1;
		
		return 0;
	}

	if (f)
	{
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
        fread(z->compressedData,1,fsize,f);
		fclose(f);
		

        blosc1_set_compressor("zstd");
        int decompressed_size = blosc2_decompress(z->compressedData, fsize, z->buffer, sizeof(ZARRType_c128i1b8)*8388608);
        if (decompressed_size < 0) {
            return 0;
        }

	}	
	
	return 0;
}
ZARRType_c128i1b8 ZARRRead_c128i1b8(ZARR_c128i1b8 *za,int x0,int x1,int x2,int x3)
{
	int c[4],m[4];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
    c[3] = x3/4;
    m[3] = x3%4;
	
	ZARRCheckChunk_c128i1b8(za,c);
	
    return (*za->buffer)[m[0]][m[1]][m[2]][m[3]];	  	  	  	  
}


// Read several values from the last dimensions
void ZARRReadN_c128i1b8(ZARR_c128i1b8 *za,int x0,int x1,int x2,int x3,int n,ZARRType_c128i1b8 *v)
{
	int c[4],m[4];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
    c[3] = x3/4;
    m[3] = x3%4;
	
	ZARRCheckChunk_c128i1b8(za,c);
			  
	memcpy(v,&(*za->buffer)[m[0]][m[1]][m[2]][m[3]],n*sizeof(ZARRType_c128i1b8));
}

int ZARRWrite_c128i1b8(ZARR_c128i1b8 *za,int x0,int x1,int x2,int x3,ZARRType_c128i1b8 value)
{
	int c[4],m[4];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
    c[3] = x3/4;
    m[3] = x3%4;
	
	ZARRCheckChunk_c128i1b8(za,c);
			  
	(*za->buffer)[m[0]][m[1]][m[2]][m[3]] = value;

	za->written[za->index] = 1;
	
	return 0;
}


void ZARRWriteN_c128i1b8(ZARR_c128i1b8 *za,int x0,int x1,int x2,int x3,int n, ZARRType_c128i1b8 *v)
{
	int c[4],m[4];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
    c[3] = x3/4;
    m[3] = x3%4;
	
	ZARRCheckChunk_c128i1b8(za,c);
			  
	memcpy(&(*za->buffer)[m[0]][m[1]][m[2]][m[3]],v,n*sizeof(ZARRType_c128i1b8));

	za->written[za->index] = 1;  
}

// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN_c128i1b8(ZARR_c128i1b8 *za,int x0,int x1,int x2,int x3,int n, ZARRType_c128i1b8 *v)
{
	int m[4];
    m[0] = x0%128;
    m[1] = x1%128;
    m[2] = x2%128;
    m[3] = x3%4;
	
	memcpy(&(*za->buffer)[m[0]][m[1]][m[2]][m[3]],v,n*sizeof(ZARRType_c128i1b8));

	za->written[za->index] = 1;  
}
