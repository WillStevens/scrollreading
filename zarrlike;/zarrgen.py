# A Zarr reader/writer generater

def DtypeToCtype(dtype):
  if dtype=="|u1":
    return "uint8_t"
  elif dtype=="<f8":
    return "float"
  else:
    exit(0)
	
def CodeGen(metadata,buffers):
  print("""
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <blosc2.h>""")

  i = 0
  for chunkDim in metadata["chunks"]:
    print("#define CHUNK_SIZE_%d %d" % (i,chunkDim))
    i += 1

  print("#define CHUNK_BYTES (1",end="")
  for i in range(0,len(metadata["chunks"])):
    print("*CHUNK_SIZE_%d" % i,end="")
  print("*sizeof(%s))" % DtypeToCtype(metadata["dtype"]))

  print("#define NUM_BUFFERS %d" % buffers)
  
  print("""
typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[CHUNK_BYTES+BLOSC2_MAX_OVERHEAD];""")
  print("    ",end="")
  print(DtypeToCtype(metadata["dtype"]),end="")
  print(""" buffers[NUM_BUFFERS]""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[CHUNK_SIZE_%d]" % i,end="")
  print(""";
    int bufferIndex[NUM_BUFFERS][""" + str(len(metadata["chunks"])) + """];
    unsigned char written[NUM_BUFFERS];
    uint64_t bufferUsed[NUM_BUFFERS];""")  
  print("    ",end="")
  print(DtypeToCtype(metadata["dtype"]),end="")
  print(" (*buffer)",end="");
  for i in range(0,len(metadata["chunks"])):
    print("[CHUNK_SIZE_%d]" % i,end="")
  print(";")
  print("""
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
      for(int j = 0; j<""" + str(len(metadata["chunks"])) + """; j++)
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
    sprintf(z->location+z->locationRootLength,"/""",end="")
  for i in range(0,len(metadata["chunks"])):
    if i!=0:
      print(metadata["dimension_separator"],end="")
    print("%d",end="")
  print('"',end="")
  for i in range(0,len(metadata["chunks"])):
    print(",z->bufferIndex[i][%d]" % i,end="")
  print(");")

  print("""
	blosc1_set_compressor("lz4");
	int compressed_len = blosc2_compress(5,1,sizeof("""+DtypeToCtype(metadata["dtype"])+"""),z->buffers[i],CHUNK_BYTES,z->compressedData,CHUNK_BYTES+BLOSC2_MAX_OVERHEAD);

    if (compressed_len <= 0) {
      return -1;
    }

	FILE *f = fopen(z->location,"wb");
	fwrite(z->compressedData,1,compressed_len,f);
	fclose(f);
	
	z->written[i] = 0;
	
	return 0;
}

int ZarrFlush(ZARR *z)
{
	for(int i = 0; i<NUM_BUFFERS; i++)
	{
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
int ZarrCheckChunk(ZARR *z, int c[""" + str(len(metadata["chunks"])) + """])
{	
	if (z->buffer""",end="")
  for i in range(0,len(metadata["chunks"])):	
    print(" && c[%d] == z->bufferIndex[z->index][%d]" % (i,i),end="")
  
  print(""")
		return 0;

	for(z->index = 0; z->index < NUM_BUFFERS; z->index++)
	{
		if (1 """,end="")
  for i in range(0,len(metadata["chunks"])):	
    print(" && c[%d] == z->bufferIndex[z->index][%d]" % (i,i),end="")

  print(""")
		{
			z->buffer = &z->buffers[z->index];
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
		printf("Ran out of buffers - flushing oldest\\n");
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
	}""")

  for i in range(0,len(metadata["chunks"])):	
    print("    z->bufferIndex[z->index][%d] = c[%d];" % (i,i))

  print("""
	z->buffer = &z->buffers[z->index];
	z->bufferUsed[z->index] = z->counter++;
	z->written[z->index] = 0;""")
	
  print( """    sprintf(z->location+z->locationRootLength,"/""",end="")
  for i in range(0,len(metadata["chunks"])):
    if i!=0:
      print(metadata["dimension_separator"],end="")
    print("%d",end="")
  print('"',end="")
  for i in range(0,len(metadata["chunks"])):
    print(",z->bufferIndex[z->index][%d]" % i,end="")
  print(");")

  print("""
    FILE *f = fopen(z->location,"rb");

    printf("Opening:%s\\n",z->location);	
	if (!f)
	{
		printf("Did not find file\\n");

		memset(z->buffer,0,CHUNK_BYTES);

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
		
		blosc1_set_compressor("lz4");
        int decompressed_size = blosc2_decompress(z->compressedData, fsize, z->buffer, CHUNK_BYTES);
        if (decompressed_size < 0) {
            return 0;
        }

	}	
	
	return 0;
}""")
  print(DtypeToCtype(metadata["dtype"]),end="")
  print(""" ZarrRead(ZARR *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""")
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/CHUNK_SIZE_%d;" % (i,i,i))
    print("    m[%d] = x%d%%CHUNK_SIZE_%d;" % (i,i,i))

  print("""	
	ZarrCheckChunk(za,c);
	
    return (*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
  print(""";	  	  	  	  
}


// Read several values from the last dimensions
void ZarrReadN(ZARR *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",int n, """ + DtypeToCtype(metadata["dtype"]) + """ *v)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/CHUNK_SIZE_%d;" % (i,i,i))
    print("    m[%d] = x%d%%CHUNK_SIZE_%d;" % (i,i,i))

  print("""	
	ZarrCheckChunk(za,c);
			  
	memcpy(v,&(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
	
  print(""",n*sizeof(float));
}

int ZarrWrite(ZARR *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",""" + DtypeToCtype(metadata["dtype"]) + """ value)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/CHUNK_SIZE_%d;" % (i,i,i))
    print("    m[%d] = x%d%%CHUNK_SIZE_%d;" % (i,i,i))

  print("""	
	ZarrCheckChunk(za,c);
			  
	(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")

  print(""" = value;

	za->written[za->index] = 1;
	
	return 0;
}


void ZarrWriteN(ZARR *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",int n, """ + DtypeToCtype(metadata["dtype"]) + """ *v)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/CHUNK_SIZE_%d;" % (i,i,i))
    print("    m[%d] = x%d%%CHUNK_SIZE_%d;" % (i,i,i))

  print("""	
	ZarrCheckChunk(za,c);
			  
	memcpy(&(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
	
  print(""",v,n*sizeof(float));

	za->written[za->index] = 1;  
}

// Assumes that we have already written at least once to this chunk
void ZarrNoCheckWriteN(ZARR *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",int n, """ + DtypeToCtype(metadata["dtype"]) + """ *v)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/CHUNK_SIZE_%d;" % (i,i,i))
    print("    m[%d] = x%d%%CHUNK_SIZE_%d;" % (i,i,i))

  print("""	
	memcpy(&za->buffer""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
	
  print(""",v,n*sizeof(float));

	za->written[za->index] = 1;  
}""")

metadata = {"chunks":[128,128,128],"dtype":"|u1", "dimension_separator": "/"}

CodeGen(metadata,8)