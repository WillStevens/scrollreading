# A ZARR reader/writer generator
import sys
import json

def DtypeToCtype(dtype):
  if type(dtype) is list:
    r = "struct {"
    for element in dtype:
      fieldName = element[0]
      fieldType = DtypeToCtype(element[1])
      r += fieldType + " " + fieldName 
      if len(element)==3:
        for dimension in element[2]:
          r += "[" + str(dimension) + "]"
      r+=";"
    r += "}"
    return r
  elif dtype=="|u1":
    return "uint8_t"
  elif dtype=="|i1":
    return "int8_t"
  elif dtype=="<f4":
    return "float"
  else:
    exit(0)

def ChunkSize(metadata,i):
  return metadata["chunks"][i]

def ChunkBytes(metadata,suffix):  
  r = "sizeof(ZARRType" + suffix + ")*"
  t = 1
  for v in metadata["chunks"]:
    t *= v
  r += str(t)
  return r

def DecompressCode(metadata,suffix):
  if metadata["compressor"]["id"] == "blosc":
    print("""
        blosc1_set_compressor(\""""+metadata["compressor"]["cname"]+"""\");
        int decompressed_size = blosc2_decompress(z->compressedData, fsize, z->buffer, """+ChunkBytes(metadata,suffix)+""");
        if (decompressed_size < 0) {
            return 0;
        }""")

def CompressCode(metadata,suffix):
  if metadata["compressor"]["id"] == "blosc":
    # TODO - the blocksize metadata value is unused
    print("""
      blosc1_set_compressor(\""""+metadata["compressor"]["cname"]+"""\");
	  int compressed_len = blosc2_compress(""" +str(metadata["compressor"]["clevel"])+ """,""" +str(metadata["compressor"]["shuffle"])+ """,sizeof(ZARRType"""+suffix+"""),z->buffers[i],"""+ChunkBytes(metadata,suffix)+""",z->compressedData,"""+ChunkBytes(metadata,suffix)+"""+BLOSC2_MAX_OVERHEAD);

      if (compressed_len <= 0) {
        return -1;
      }""")

  
def CodeGen(metadata,buffers,suffix):
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

  print("""typedef """ + DtypeToCtype(metadata["dtype"]) + """ ZARRType"""+suffix+""";""")
  
  print("""
typedef struct {
    int locationRootLength;
    char *location;
  
    unsigned char compressedData[""" + ChunkBytes(metadata,suffix) + """+BLOSC2_MAX_OVERHEAD];""")
  print("    ",end="")
  print("""ZARRType"""+suffix,end="")
  print(""" buffers[""" + str(buffers) + """]""",end="")
  for v in metadata["chunks"]:
    print("[%d]" % v,end="")
  print(""";
    int bufferIndex[""" + str(buffers) + """][""" + str(len(metadata["chunks"])) + """];
    unsigned char written[""" + str(buffers) + """];
    uint64_t bufferUsed[""" + str(buffers) + """];""")  
  print("    ",end="")
  print("""ZARRType"""+suffix,end="")
  print(" (*buffer)",end="");
  for v in metadata["chunks"]:
    print("[%d]" % v,end="")
  print(";")
  print("""
    int index;
  
    uint64_t counter;
} ZARR"""+suffix+""";

ZARR"""+suffix+""" *ZARROpen"""+suffix+"""(const char *location)
{
	ZARR"""+suffix+""" *z = (ZARR"""+suffix+""" *)malloc(sizeof(ZARR"""+suffix+"""));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
	z->buffer = NULL;
	z->index = -1;

    for(int i = 0; i<""" + str(buffers) + """; i++)
	{
	  z->written[i] = 0;
      for(int j = 0; j<""" + str(len(metadata["chunks"])) + """; j++)
        z->bufferIndex[i][j] = -1;
	}
	
    strcpy(z->location,location);
	
	z->counter = 1;

    for(int i = 0; i<""" + str(buffers) + """; i++)
      z->bufferUsed[i] = 0;
  
	return z;
}

int ZARRFlushOne"""+suffix+"""(ZARR"""+suffix+""" *z, int i)
{
    if (z->written[i])
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

  CompressCode(metadata,suffix)
  
  print("""
	  FILE *f = fopen(z->location,"wb");
	  fwrite(z->compressedData,1,compressed_len,f);
	  fclose(f);
	
	  z->written[i] = 0;
	}
	
	return 0;
}

int ZARRFlush"""+suffix+"""(ZARR"""+suffix+""" *z)
{
	for(int i = 0; i<"""+str(buffers)+"""; i++)
	{
		if (z->bufferIndex[i][0] != -1)
		{
			ZARRFlushOne"""+suffix+"""(z,i);
		}
	}
	
	return 0;
}

int ZARRClose"""+suffix+"""(ZARR"""+suffix+""" *z)
{
    ZARRFlush"""+suffix+"""(z);
    free(z->location);
    free(z);
	
	return 0;
}

/* Make buffer point to the chunk and index contain the index*/
int ZARRCheckChunk"""+suffix+"""(ZARR"""+suffix+""" *z, int c[""" + str(len(metadata["chunks"])) + """])
{	
	if (z->buffer""",end="")
  for i in range(0,len(metadata["chunks"])):	
    print(" && c[%d] == z->bufferIndex[z->index][%d]" % (i,i),end="")
  
  print(""")
		return 0;

	for(z->index = 0; z->index < """+str(buffers)+"""; z->index++)
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
		
	for(z->index = 0; z->index < """ + str(buffers) + """; z->index++)
	{
		if (z->bufferIndex[z->index][0]==-1)
			break;
	}
  	
	if (z->index == """+str(buffers)+""")
	{
		/* Find the buffer that was least recently used and free it up */
		printf("Ran out of buffers - flushing oldest\\n");
		int oldestIndex = 0;
		uint64_t oldestAge = z->bufferUsed[0];
		for(int i = 1; i<"""+str(buffers)+"""; i++)
		{
			if (z->bufferUsed[i]<oldestAge)
			{
				oldestAge = z->bufferUsed[i];
				oldestIndex = i;
			}
		}
		
		ZARRFlushOne"""+suffix+"""(z,oldestIndex);
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

		memset(z->buffer,0,"""+ChunkBytes(metadata,suffix)+""");

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
		""")
  DecompressCode(metadata,suffix)
  print("""
	}	
	
	return 0;
}""")
  print("""ZARRType"""+suffix,end="")
  print(""" ZARRRead"""+suffix+"""(ZARR"""+suffix+""" *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""")
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/%d;" % (i,i,ChunkSize(metadata,i)))
    print("    m[%d] = x%d%%%d;" % (i,i,ChunkSize(metadata,i)))

  print("""	
	ZARRCheckChunk"""+suffix+"""(za,c);
	
    return (*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
  print(""";	  	  	  	  
}


// Read several values from the last dimensions
void ZARRReadN"""+suffix+"""(ZARR"""+suffix+""" *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",int n,ZARRType"""+suffix + """ *v)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/%d;" % (i,i,ChunkSize(metadata,i)))
    print("    m[%d] = x%d%%%d;" % (i,i,ChunkSize(metadata,i)))

  print("""	
	ZARRCheckChunk"""+suffix+"""(za,c);
			  
	memcpy(v,&(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
	
  print(""",n*sizeof(ZARRType""" + suffix + """));
}

int ZARRWrite"""+suffix+"""(ZARR"""+suffix+""" *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",ZARRType"""+suffix + """ value)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/%d;" % (i,i,ChunkSize(metadata,i)))
    print("    m[%d] = x%d%%%d;" % (i,i,ChunkSize(metadata,i)))

  print("""	
	ZARRCheckChunk"""+suffix+"""(za,c);
			  
	(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")

  print(""" = value;

	za->written[za->index] = 1;
	
	return 0;
}


void ZARRWriteN"""+suffix+"""(ZARR"""+suffix+""" *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",int n, ZARRType"""+suffix + """ *v)
{
	int c[""" + str(len(metadata["chunks"])) + """],m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    c[%d] = x%d/%d;" % (i,i,ChunkSize(metadata,i)))
    print("    m[%d] = x%d%%%d;" % (i,i,ChunkSize(metadata,i)))

  print("""	
	ZARRCheckChunk"""+suffix+"""(za,c);
			  
	memcpy(&(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
	
  print(""",v,n*sizeof(ZARRType""" + suffix + """));

	za->written[za->index] = 1;  
}

// Assumes that we have already written at least once to this chunk
void ZARRNoCheckWriteN"""+suffix+"""(ZARR"""+suffix+""" *za""",end="")
  for i in range(0,len(metadata["chunks"])):
    print(",int x%d" % i,end="")
  print(""",int n, ZARRType"""+suffix + """ *v)
{
	int m[""" + str(len(metadata["chunks"])) + """];""")

  for i in range(0,len(metadata["chunks"])):
    print("    m[%d] = x%d%%%d;" % (i,i,ChunkSize(metadata,i)))

  print("""	
	memcpy(&(*za->buffer)""",end="")
  for i in range(0,len(metadata["chunks"])):
    print("[m[%d]]" % i,end="")
	
  print(""",v,n*sizeof(ZARRType""" + suffix + """));

	za->written[za->index] = 1;  
}""")

if len(sys.argv) != 4:
  sys.stderr.write("Usage: zarrgen.py <zarr metadata file> <buffers> <suffix>\n")
  sys.stderr.write("Outputs C code on standard output for reading/writing zarr")
  exit(1)

with open(sys.argv[1]) as file:
  data = file.read()
  metadata = json.loads(data)
  
  CodeGen(metadata,int(sys.argv[2]),sys.argv[3])

exit(0)