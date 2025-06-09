#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <blosc2.h>

#define CHUNK_SIZE_X 128
#define CHUNK_SIZE_Y 128
#define CHUNK_SIZE_Z 128
#define CHUNK_SIZE_W 4

#define CHUNK_BYTES (CHUNK_SIZE_Z*CHUNK_SIZE_Y*CHUNK_SIZE_Z*CHUNK_SIZE_W*sizeof(float))

#define NUM_BUFFERS 8

typedef struct {
  int locationRootLength;
  char *location;
  
  unsigned char compressedData[CHUNK_BYTES+BLOSC2_MAX_OVERHEAD];
  unsigned char buffers[NUM_BUFFERS][CHUNK_BYTES];
  int bufferIndex[NUM_BUFFERS][4];
  unsigned char written[NUM_BUFFERS];
  uint64_t bufferUsed[NUM_BUFFERS];  
  unsigned char *buffer;
  int index;
  
  uint64_t counter;
} ZARR;

ZARR *ZarrOpen(const char *location)
{
	ZARR *z = (ZARR *)malloc(sizeof(ZARR));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<NUM_BUFFERS; i++)
	{
	  z->written[i] = 0;
      for(int j = 0; j<4; j++)
        z->bufferIndex[i][j] = -1;
	}
	
    strcpy(z->location,location);
	
	z->counter = 1;

    for(int i = 0; i<NUM_BUFFERS; i++)
      z->bufferUsed[i] = 0;
  
	return z;
}

int ZarrFlushOne(ZARR *z, int i)
{
	printf("flushing\n");
	if (z->written[i])
	{
		sprintf(z->location+z->locationRootLength,"/%d.%d.%d.%d/data.bin",z->bufferIndex[i][0],z->bufferIndex[i][1],z->bufferIndex[i][2],z->bufferIndex[i][3]);

		printf("compressing\n");
	    blosc1_set_compressor("lz4");
		int compressed_len = blosc2_compress(9,1,sizeof(float),z->buffers[i],CHUNK_BYTES,z->compressedData,CHUNK_BYTES+BLOSC2_MAX_OVERHEAD);

		if (compressed_len <= 0) {
		//LOG_ERROR("Blosc2 compression failed: %d\n", compressed_len);
		return -1;
		}

		printf("saving\n");

		FILE *f = fopen(z->location,"wb");
		fwrite(z->compressedData,1,compressed_len,f);
		fclose(f);
	
		z->written[i] = 0;
	}
	printf("done\n");
	
	return 0;
}

int ZarrFlush(ZARR *z)
{
	for(int i = 0; i<NUM_BUFFERS; i++)
	{
		printf("Flushing %d\n",i);
		if (z->bufferIndex[i][0] != -1)
		{
			ZarrFlushOne(z,i);
		}
	}
	
	return 0;
}

int ZarrClose(ZARR *z)
{
   ZarrFlush(z);
   free(z->location);
   free(z);
	
	return 0;
}

/* Make buffer point to the chunk and index contain the index*/
int ZarrCheckChunk(ZARR *z, int c[4])
{	
	if (z->buffer && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2] && c[3] == z->bufferIndex[z->index][3])
		return 0;

	for(z->index = 0; z->index < NUM_BUFFERS; z->index++)
	{
		if (c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2] && c[3] == z->bufferIndex[z->index][3])
		{
			z->buffer = z->buffers[z->index];
			z->bufferUsed[z->index] = z->counter++;
			return 0;
		}
	}
		
	for(z->index = 0; z->index < NUM_BUFFERS; z->index++)
	{
		if (z->bufferIndex[z->index][0]==-1)
			break;
	}
  	
	if (z->index == NUM_BUFFERS)
	{
		/* Find the buffer that was least recently used and free it up */
		printf("Ran out of buffers - flushing oldest\n");
		int oldestIndex = 0;
		int oldestAge = z->bufferUsed[0];
		for(int i = 1; i<NUM_BUFFERS; i++)
		{
			if (z->bufferUsed[i]<oldestAge)
			{
				oldestAge = z->bufferUsed[i];
				oldestIndex = i;
			}
		}
		
		ZarrFlushOne(z,oldestIndex);
		z->index = oldestIndex;
	}

	z->bufferIndex[z->index][0] = c[0];
	z->bufferIndex[z->index][1] = c[1];
	z->bufferIndex[z->index][2] = c[2];
	z->bufferIndex[z->index][3] = c[3];
	z->buffer = z->buffers[z->index];
	z->bufferUsed[z->index] = z->counter++;
	z->written[z->index] = 0;
	
	sprintf(z->location+z->locationRootLength,"/%d.%d.%d.%d/data.bin",c[0],c[1],c[2],c[3]);
	FILE *f = fopen(z->location,"rb");
		
	if (!f)
	{
		printf("Did not find file\n");
		/* Make new directory */
		int finalForwardSlash = strlen(z->location)-9;
		*(z->location+finalForwardSlash)=0;
		mkdir(z->location,0777);	  
		*(z->location+finalForwardSlash)='/';

		memset(z->buffer,0,CHUNK_BYTES);

	    z->written[z->index] = 1;
		
		return 0;
	}

	if (f)
	{
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
        fread(z->compressedData,1,fsize,f);
		fclose(f);

	    blosc1_set_compressor("lz4");		
        int decompressed_size = blosc2_decompress(z->compressedData, fsize, z->buffer, CHUNK_BYTES);
        if (decompressed_size < 0) {
            // LOG_ERROR("Blosc2 decompression failed: %d\n", decompressed_size);
            return 0;
        }

	}	
	
	return 0;
}

float ZarrRead(ZARR *za, int x, int y, int z, int w)
{
	int c[4],m[4];
	
	c[0] = z/CHUNK_SIZE_Z;
	c[1] = y/CHUNK_SIZE_Y;
	c[2] = x/CHUNK_SIZE_X;
	c[3] = w/CHUNK_SIZE_W;
	m[0] = z%CHUNK_SIZE_Z;
	m[1] = y%CHUNK_SIZE_Y;
	m[2] = x%CHUNK_SIZE_X;
	m[3] = w%CHUNK_SIZE_W;
	
	ZarrCheckChunk(za,c);
		
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	float value;
	  
	memcpy(&value,za->buffer+pos,sizeof(float));
	  	  	  	  
	return value;
}


// Read several values from the last dimensions
void ZarrReadN(ZARR *za, int x, int y, int z, int w, int n, float *v)
{
	int c[4],m[4];
	
	c[0] = z/CHUNK_SIZE_Z;
	c[1] = y/CHUNK_SIZE_Y;
	c[2] = x/CHUNK_SIZE_X;
	c[3] = w/CHUNK_SIZE_W;
	m[0] = z%CHUNK_SIZE_Z;
	m[1] = y%CHUNK_SIZE_Y;
	m[2] = x%CHUNK_SIZE_X;
	m[3] = w%CHUNK_SIZE_W;
	
	ZarrCheckChunk(za,c);
		
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	  
	memcpy(v,za->buffer+pos,n*sizeof(float));
}

int ZarrWrite(ZARR *za, int x, int y, int z, int w, float value)
{
	int c[4],m[4];
	
	c[0] = z/CHUNK_SIZE_Z;
	c[1] = y/CHUNK_SIZE_Y;
	c[2] = x/CHUNK_SIZE_X;
	c[3] = w/CHUNK_SIZE_W;
	m[0] = z%CHUNK_SIZE_Z;
	m[1] = y%CHUNK_SIZE_Y;
	m[2] = x%CHUNK_SIZE_X;
	m[3] = w%CHUNK_SIZE_W;
	
	ZarrCheckChunk(za,c);
	
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	  
	memcpy(za->buffer+pos,&value,sizeof(float));
	za->written[za->index] = 1;
	
	return 0;
}


void ZarrWriteN(ZARR *za, int x, int y, int z, int w, int n, float *v)
{
	int c[4],m[4];
	
	c[0] = z/CHUNK_SIZE_Z;
	c[1] = y/CHUNK_SIZE_Y;
	c[2] = x/CHUNK_SIZE_X;
	c[3] = w/CHUNK_SIZE_W;
	m[0] = z%CHUNK_SIZE_Z;
	m[1] = y%CHUNK_SIZE_Y;
	m[2] = x%CHUNK_SIZE_X;
	m[3] = w%CHUNK_SIZE_W;
	
	ZarrCheckChunk(za,c);
	
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	  	  
	memcpy(za->buffer+pos,v,n*sizeof(float));
	za->written[za->index] = 1;
	
}

// Assumes that we have already written at least once to this chunk
void ZarrNoCheckWriteN(ZARR *za, int x, int y, int z, int w, int n, float *v)
{
	int m[4];
	
	m[0] = z%CHUNK_SIZE_Z;
	m[1] = y%CHUNK_SIZE_Y;
	m[2] = x%CHUNK_SIZE_X;
	m[3] = w%CHUNK_SIZE_W;
		
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	  
	memcpy(za->buffer+pos,v,n*sizeof(float));
}
