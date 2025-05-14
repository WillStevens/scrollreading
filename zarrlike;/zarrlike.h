#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CHUNK_SIZE_X 128
#define CHUNK_SIZE_Y 128
#define CHUNK_SIZE_Z 128
#define CHUNK_SIZE_W 4

#define CHUNK_BYTES (CHUNK_SIZE_Z*CHUNK_SIZE_Y*CHUNK_SIZE_Z*CHUNK_SIZE_W*sizeof(float))

#define NUM_BUFFERS 8

typedef struct {
  int locationRootLength;
  char *location;
  
  unsigned char buffers[NUM_BUFFERS][CHUNK_BYTES];
  int bufferIndex[NUM_BUFFERS][4];  
  unsigned char *buffer;
  int index;
} ZARR;

ZARR *ZarrOpen(const char *location)
{
	ZARR *z = (ZARR *)malloc(sizeof(ZARR));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<8; i++)
    for(int j = 0; j<4; j++)
      z->bufferIndex[i][j] = -1;
	
    strcpy(z->location,location);
	
	return z;
}

int ZarrFlush(ZARR *z)
{
	for(int i = 0; i<NUM_BUFFERS; i++)
	{
		if (z->bufferIndex[i][0] != -1)
		{
			sprintf(z->location+z->locationRootLength,"/%d.%d.%d.%d/data.bin",z->bufferIndex[i][0],z->bufferIndex[i][1],z->bufferIndex[i][2],z->bufferIndex[i][3]);

			FILE *f = fopen(z->location,"wb");
			fwrite(z->buffers[i],1,CHUNK_BYTES,f);
			fclose(f);
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
		printf("Ran out of buffers\n");
		exit(0);
	}

	z->bufferIndex[z->index][0] = c[0];
	z->bufferIndex[z->index][1] = c[1];
	z->bufferIndex[z->index][2] = c[2];
	z->bufferIndex[z->index][3] = c[3];
	z->buffer = z->buffers[z->index];
	
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

		f = fopen(z->location,"wb");

		float zero = 0;
	  
		for(int i = 0; i<CHUNK_SIZE_Z*CHUNK_SIZE_Y*CHUNK_SIZE_Z*CHUNK_SIZE_W; i++)
			fwrite(&zero,sizeof(float),1,f);
            
		fclose(f);

		f = fopen(z->location,"rb");
	}

	if (f)
	{
        fread(z->buffer,1,CHUNK_BYTES,f);
	}	
	
	return 0;
}

float ZarrRead(ZARR *z, int *index)
{
	int c[4],m[4];
	
	c[0] = index[0]/CHUNK_SIZE_Z;
	c[1] = index[1]/CHUNK_SIZE_Y;
	c[2] = index[2]/CHUNK_SIZE_X;
	c[3] = index[3]/CHUNK_SIZE_W;
	m[0] = index[0]%CHUNK_SIZE_Z;
	m[1] = index[1]%CHUNK_SIZE_Y;
	m[2] = index[2]%CHUNK_SIZE_X;
	m[3] = index[3]%CHUNK_SIZE_W;
	
	ZarrCheckChunk(z,c);
		
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	float value;
	  
	memcpy(&value,z->buffer+pos,sizeof(float));
	  	  	  	  
	return value;
}

int ZarrWrite(ZARR *z, int *index, float value)
{
	int c[4],m[4];
	
	c[0] = index[0]/CHUNK_SIZE_Z;
	c[1] = index[1]/CHUNK_SIZE_Y;
	c[2] = index[2]/CHUNK_SIZE_X;
	c[3] = index[3]/CHUNK_SIZE_W;
	m[0] = index[0]%CHUNK_SIZE_Z;
	m[1] = index[1]%CHUNK_SIZE_Y;
	m[2] = index[2]%CHUNK_SIZE_X;
	m[3] = index[3]%CHUNK_SIZE_W;
	
	ZarrCheckChunk(z,c);
	
    int pos = m[0]*CHUNK_SIZE_Y*CHUNK_SIZE_X*CHUNK_SIZE_W+m[1]*CHUNK_SIZE_X*CHUNK_SIZE_W+m[2]*CHUNK_SIZE_W+m[3];
	pos *= sizeof(float);
	  
	memcpy(z->buffer+pos,&value,sizeof(float));
	
	return 0;
}
