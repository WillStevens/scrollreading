
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <blosc2.h>
typedef uint16_t ZARRType_4;

typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[sizeof(ZARRType_4)*2097152+BLOSC2_MAX_OVERHEAD];
    ZARRType_4 buffers[80][128][128][128];
    int bufferIndex[80][3];
    unsigned char written[80];
    uint64_t bufferUsed[80];
    ZARRType_4 (*buffer)[128][128][128];

    int index;
  
    uint64_t counter;
} ZARR_4;

ZARR_4 *ZARROpen_4(const char *location)
{
	ZARR_4 *z = (ZARR_4 *)malloc(sizeof(ZARR_4));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<80; i++)
	{
	  z->written[i] = 0;
      for(int j = 0; j<3; j++)
        z->bufferIndex[i][j] = -1;
	}
	
    strcpy(z->location,location);
	
	z->counter = 1;

    for(int i = 0; i<80; i++)
      z->bufferUsed[i] = 0;
  
	return z;
}

int ZARRFlushOne_4(ZARR_4 *z, int i)
{
    if (z->written[i])
	{
      sprintf(z->location+z->locationRootLength,"/%d/%d/%d",z->bufferIndex[i][0],z->bufferIndex[i][1],z->bufferIndex[i][2]);

	  int compressed_len = sizeof(ZARRType_4)*2097152;
	  memcpy(z->compressedData,z->buffer,compressed_len);
	  

	  FILE *f = fopen(z->location,"wb");
	  fwrite(z->compressedData,1,compressed_len,f);
	  fclose(f);
	
	  z->written[i] = 0;
	}
	
	return 0;
}

int ZARRFlush_4(ZARR_4 *z)
{
	for(int i = 0; i<80; i++)
	{
		if (z->bufferIndex[i][0] != -1)
		{
			ZARRFlushOne_4(z,i);
		}
	}
	
	return 0;
}

int ZARRClose_4(ZARR_4 *z)
{
    ZARRFlush_4(z);
    free(z->location);
    free(z);
	
	return 0;
}

/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_4(ZARR_4 *z, int c[3])
{	
	if (z->buffer && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2])
		return 0;

	for(z->index = 0; z->index < 80; z->index++)
	{
		if (1  && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2])
		{
			z->buffer = &z->buffers[z->index];
			z->bufferUsed[z->index] = z->counter++;
			return 0;
		}
	}
		
	for(z->index = 0; z->index < 80; z->index++)
	{
		if (z->bufferIndex[z->index][0]==-1)
			break;
	}
  	
	if (z->index == 80)
	{
		/* Find the buffer that was least recently used and free it up */
		printf("Ran out of buffers - flushing oldest\n");
		int oldestIndex = 0;
		uint64_t oldestAge = z->bufferUsed[0];
		for(int i = 1; i<80; i++)
		{
			if (z->bufferUsed[i]<oldestAge)
			{
				oldestAge = z->bufferUsed[i];
				oldestIndex = i;
			}
		}
		
		ZARRFlushOne_4(z,oldestIndex);
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

    //printf("Opening:%s\n",z->location);	
	if (!f)
	{
		printf("Did not find file:%s\n",z->location); // Useful to display this message because it often indicates a file naming problem

		memset(z->buffer,0,sizeof(ZARRType_4)*2097152);

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
		

	  memcpy(z->buffer,z->compressedData,sizeof(ZARRType_4)*2097152);
	  

	}	
	
	return 0;
}
ZARRType_4 ZARRRead_4(ZARR_4 *za,int x0,int x1,int x2)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_4(za,c);
	
    return (*za->buffer)[m[0]][m[1]][m[2]];	  	  	  	  
}


// Read several values from the last dimensions
void ZARRReadN_4(ZARR_4 *za,int x0,int x1,int x2,int n,ZARRType_4 *v)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_4(za,c);
			  
	memcpy(v,&(*za->buffer)[m[0]][m[1]][m[2]],n*sizeof(ZARRType_4));
}

int ZARRWrite_4(ZARR_4 *za,int x0,int x1,int x2,ZARRType_4 value)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_4(za,c);
			  
	(*za->buffer)[m[0]][m[1]][m[2]] = value;

	za->written[za->index] = 1;
	
	return 0;
}


void ZARRWriteN_4(ZARR_4 *za,int x0,int x1,int x2,int n, ZARRType_4 *v)
{
	int c[3],m[3];
    c[0] = x0/128;
    m[0] = x0%128;
    c[1] = x1/128;
    m[1] = x1%128;
    c[2] = x2/128;
    m[2] = x2%128;
	
	ZARRCheckChunk_4(za,c);
			  
	memcpy(&(*za->buffer)[m[0]][m[1]][m[2]],v,n*sizeof(ZARRType_4));

	za->written[za->index] = 1;  
}

// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN_4(ZARR_4 *za,int x0,int x1,int x2,int n, ZARRType_4 *v)
{
	int m[3];
    m[0] = x0%128;
    m[1] = x1%128;
    m[2] = x2%128;
	
	memcpy(&(*za->buffer)[m[0]][m[1]][m[2]],v,n*sizeof(ZARRType_4));

	za->written[za->index] = 1;  
}
