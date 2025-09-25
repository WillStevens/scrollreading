
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <blosc2.h>

typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[sizeof(uint8_t)*2097152+BLOSC2_MAX_OVERHEAD];
    uint8_t buffers[27][128][128][128];
    int bufferIndex[27][3];
    unsigned char written[27];
    uint64_t bufferUsed[27];
    uint8_t (*buffer)[128][128][128];

    int index;
  
    uint64_t counter;
} ZARR_1;

ZARR_1 *ZARROpen_1(const char *location)
{
	ZARR_1 *z = (ZARR_1 *)malloc(sizeof(ZARR_1));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<27; i++)
	{
	  z->written[i] = 0;
      for(int j = 0; j<3; j++)
        z->bufferIndex[i][j] = -1;
	}
	
    strcpy(z->location,location);
	
	z->counter = 1;

    for(int i = 0; i<27; i++)
      z->bufferUsed[i] = 0;
  
	return z;
}

int ZARRFlushOne_1(ZARR_1 *z, int i)
{
    if (z->written[i])
	{
      sprintf(z->location+z->locationRootLength,"/%d/%d/%d",z->bufferIndex[i][0],z->bufferIndex[i][1],z->bufferIndex[i][2]);

	  blosc1_set_compressor("zstd");
	  int compressed_len = blosc2_compress(3,1,sizeof(uint8_t),z->buffers[i],sizeof(uint8_t)*2097152,z->compressedData,sizeof(uint8_t)*2097152+BLOSC2_MAX_OVERHEAD);

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

int ZARRFlush_1(ZARR_1 *z)
{
	for(int i = 0; i<27; i++)
	{
		if (z->bufferIndex[i][0] != -1)
		{
			ZARRFlushOne_1(z,i);
		}
	}
	
	return 0;
}

int ZARRClose_1(ZARR_1 *z)
{
    ZARRFlush_1(z);
    free(z->location);
    free(z);
	
	return 0;
}

/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_1(ZARR_1 *z, int c[3])
{	
	if (z->buffer && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2])
		return 0;

	for(z->index = 0; z->index < 27; z->index++)
	{
		if (1  && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2])
		{
			z->buffer = &z->buffers[z->index];
			z->bufferUsed[z->index] = z->counter++;
			return 0;
		}
	}
		
	for(z->index = 0; z->index < 27; z->index++)
	{
		if (z->bufferIndex[z->index][0]==-1)
			break;
	}
  	
	if (z->index == 27)
	{
		/* Find the buffer that was least recently used and free it up */
		//printf("Ran out of buffers - flushing oldest\n");
		int oldestIndex = 0;
		uint64_t oldestAge = z->bufferUsed[0];
		for(int i = 1; i<27; i++)
		{
			if (z->bufferUsed[i]<oldestAge)
			{
				oldestAge = z->bufferUsed[i];
				oldestIndex = i;
			}
		}
		
		ZARRFlushOne_1(z,oldestIndex);
		z->index = oldestIndex;
	}
    z->bufferIndex[z->index][0] = c[0];
    z->bufferIndex[z->index][1] = c[1];
    z->bufferIndex[z->index][2] = c[2];

	z->buffer = &z->buffers[z->index];
	z->bufferUsed[z->index] = z->counter++;
	z->written[z->index] = 0;
    sprintf(z->location+z->locationRootLength,"/%d/%d/%d",z->bufferIndex[z->index][0],z->bufferIndex[z->index][1],z->bufferIndex[z->index][2]);

    FILE *f = fopen(z->location,"rb");

    printf("Opening:%s\n",z->location);	
	if (!f)
	{
		printf("Did not find file\n");

		memset(z->buffer,0,sizeof(uint8_t)*2097152);

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
        int decompressed_size = blosc2_decompress(z->compressedData, fsize, z->buffer, sizeof(uint8_t)*2097152);
        if (decompressed_size < 0) {
            return 0;
        }

	}	
	
	return 0;
}
uint8_t ZARRRead_1(ZARR_1 *za,int x0,int x1,int x2)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_1(za,c);
	
    return (*za->buffer)[m[0]][m[1]][m[2]];	  	  	  	  
}


// Read several values from the last dimensions
void ZARRReadN_1(ZARR_1 *za,int x0,int x1,int x2,int n, uint8_t *v)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_1(za,c);
			  
	memcpy(v,&(*za->buffer)[m[0]][m[1]][m[2]],n*sizeof(float));
}

int ZARRWrite_1(ZARR_1 *za,int x0,int x1,int x2,uint8_t value)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_1(za,c);
			  
	(*za->buffer)[m[0]][m[1]][m[2]] = value;

	za->written[za->index] = 1;
	
	return 0;
}


void ZARRWriteN_1(ZARR_1 *za,int x0,int x1,int x2,int n, uint8_t *v)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_1(za,c);
			  
	memcpy(&(*za->buffer)[m[0]][m[1]][m[2]],v,n*sizeof(float));

	za->written[za->index] = 1;  
}

// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN_1(ZARR_1 *za,int x0,int x1,int x2,int n, uint8_t *v)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	memcpy(&(*za->buffer)[m[0]][m[1]][m[2]],v,n*sizeof(float));

	za->written[za->index] = 1;  
}
