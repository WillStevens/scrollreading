/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include <blosc2.h>
#include <blosc.h>

#define CHUNK_SIZE 128*128*128

uint8_t buffer1[CHUNK_SIZE];
uint8_t buffer2[CHUNK_SIZE];
uint8_t buffer3[CHUNK_SIZE];

uint8_t compressed[CHUNK_SIZE+BLOSC2_MAX_OVERHEAD];

int ReadIntoBuffer(char *fname,void *buffer)
{
    FILE *f = fopen(fname,"rb");
	
	if (f)
	{
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
        fread((void *)compressed,1,fsize,f);
		fclose(f);
		

        blosc1_set_compressor("zstd");
        int decompressed_size = blosc2_decompress((void *)compressed, fsize, buffer, CHUNK_SIZE);
        if (decompressed_size < 0) {
            return 0;
        }
	}	
	
	return 1;
}

int WriteFromBuffer(char *fname,void *buffer)
{
	blosc_set_compressor("zstd");
	int compressed_len = blosc_compress(3,1,1,CHUNK_SIZE,buffer,(void *)compressed,CHUNK_SIZE+BLOSC_MAX_OVERHEAD);

    if (compressed_len <= 0) {
        return -1;
    }

	FILE *f = fopen(fname,"wb");
	fwrite((void *)compressed,1,compressed_len,f);
	fclose(f);
	
	return 0;
}

int main(int argc, char *argv[])
{		
    if (argc != 4)
	{
		printf("Usage: zarrand source1 source2 dest\n");
		exit(-1);
	}
	
	blosc_init();
	blosc2_init();
	
	
	if (ReadIntoBuffer(argv[1],(void *)buffer1) && ReadIntoBuffer(argv[2],(void *)buffer2))
	{
		for(int i = 0; i<CHUNK_SIZE; i++)
		{
			if (buffer1[i]==255 && buffer2[i]==255)
				buffer3[i]=255;
		}
		
		WriteFromBuffer(argv[3],(void *)buffer3);
	}
	else
	{
		printf("At least one input not found\n");
	}

	blosc2_destroy();
	blosc_destroy();
}

