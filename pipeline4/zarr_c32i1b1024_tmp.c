
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <blosc2.h>

#include <unordered_map>

typedef std::unordered_map<uint64_t,int> bufferIndexLookupType;

typedef int8_t ZARRType_c32i1b1024;

typedef struct {
    int locationRootLength;
    char *location;

 	
    unsigned char compressedData[sizeof(ZARRType_c32i1b1024)*131072+BLOSC2_MAX_OVERHEAD];
    ZARRType_c32i1b1024 buffers[1024][32][32][32][4];
    int bufferIndex[1024][4];
	bufferIndexLookupType *bufferIndexLookup;
    unsigned char written[1024];
    uint64_t bufferUsed[1024];
    ZARRType_c32i1b1024 (*buffer)[32][32][32][4];

    int index;
  
    uint64_t counter;
} ZARR_c32i1b1024;

ZARR_c32i1b1024 *ZARROpen_c32i1b1024(const char *location)
{
	ZARR_c32i1b1024 *z = (ZARR_c32i1b1024 *)malloc(sizeof(ZARR_c32i1b1024));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->bufferIndexLookup = new bufferIndexLookupType;
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<1024; i++)
	{
	  z->written[i] = 0;
      for(int j = 0; j<4; j++)
        z->bufferIndex[i][j] = -1;
	}
	
    strcpy(z->location,location);
	
	z->counter = 1;

    for(int i = 0; i<1024; i++)
      z->bufferUsed[i] = 0;
  
	return z;
}

int ZARRClose_c32i1b1024(ZARR_c32i1b1024 *z)
{
    delete z->bufferIndexLookup;
    free(z->location);
    free(z);
	
	return 0;
}

/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk_c32i1b1024(ZARR_c32i1b1024 *z, int c[4])
{	
	if (z->buffer && c[0] == z->bufferIndex[z->index][0] && c[1] == z->bufferIndex[z->index][1] && c[2] == z->bufferIndex[z->index][2] && c[3] == z->bufferIndex[z->index][3])
		return 0;

	uint64_t ct = ((uint64_t)(c[0])<<48)+((uint64_t)(c[1])<<32)+((uint64_t)(c[2])<<16)+(uint64_t)(c[3]);
	
	if (z->bufferIndexLookup->count(ct)==1)
	{
		z->index = (*z->bufferIndexLookup)[ct];
		z->buffer = &z->buffers[z->index];
		z->bufferUsed[z->index] = z->counter++;
		return 0;
	}
		
	z->index = z->bufferIndexLookup->size();
  	
	if (z->index >= 1024)
	{
		printf("Ran out of buffers\n");
		exit(-1);
	}
    z->bufferIndex[z->index][0] = c[0];
    z->bufferIndex[z->index][1] = c[1];
    z->bufferIndex[z->index][2] = c[2];
    z->bufferIndex[z->index][3] = c[3];
    
	(*z->bufferIndexLookup)[ct] = z->index;
	
	z->buffer = &z->buffers[z->index];
	z->bufferUsed[z->index] = z->counter++;
	z->written[z->index] = 0;
    sprintf(z->location+z->locationRootLength,"/%d.%d.%d.%d",z->bufferIndex[z->index][0],z->bufferIndex[z->index][1],z->bufferIndex[z->index][2],z->bufferIndex[z->index][3]);

    FILE *f = fopen(z->location,"rb");

    //printf("Opening:%s\n",z->location);	
	if (!f)
	{
		//printf("Did not find file\n");

		memset(z->buffer,0,sizeof(ZARRType_c32i1b1024)*131072);

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
        int decompressed_size = blosc2_decompress(z->compressedData, fsize, z->buffer, sizeof(ZARRType_c32i1b1024)*131072);
        if (decompressed_size < 0) {
            return 0;
        }

	}	
	
	return 0;
}
ZARRType_c32i1b1024 ZARRRead_c32i1b1024(ZARR_c32i1b1024 *za,int x0,int x1,int x2,int x3)
{
	int c[4],m[4];
    c[0] = x0/32;
    m[0] = x0%32;
    c[1] = x1/32;
    m[1] = x1%32;
    c[2] = x2/32;
    m[2] = x2%32;
    c[3] = x3/4;
    m[3] = x3%4;
	
	ZARRCheckChunk_c32i1b1024(za,c);
	
    return (*za->buffer)[m[0]][m[1]][m[2]][m[3]];	  	  	  	  
}


// Read several values from the last dimensions
void ZARRReadN_c32i1b1024(ZARR_c32i1b1024 *za,int x0,int x1,int x2,int x3,int n,ZARRType_c32i1b1024 *v)
{
	int c[4],m[4];
    c[0] = x0/32;
    m[0] = x0%32;
    c[1] = x1/32;
    m[1] = x1%32;
    c[2] = x2/32;
    m[2] = x2%32;
    c[3] = x3/4;
    m[3] = x3%4;
	
	ZARRCheckChunk_c32i1b1024(za,c);
			  
	memcpy(v,&(*za->buffer)[m[0]][m[1]][m[2]][m[3]],n*sizeof(ZARRType_c32i1b1024));
}

