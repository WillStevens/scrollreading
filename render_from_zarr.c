/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/


#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#define VESUVIUS_IMPL
#include "../vesuvius-c/vesuvius-c.h"

#define SHEET_SIZE 509

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

unsigned char rendered[SHEET_SIZE][SHEET_SIZE];

void render(char *fname)
{
	float xo,yo,zo;
	int x,y,z;
		
	for(int y=0; y<SHEET_SIZE; y++)
	for(int x=0; x<SHEET_SIZE; x++)
		rendered[y][x] = 0;

    //initialize the volume
    volume* scroll_vol = vs_vol_new(
        "d:/zarrs/54keV_7.91um_Scroll1A.zarr/0/",
        "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/");
	
    FILE *f = fopen(fname,"r");
	
	chunk *currentChunk = NULL;
	int cx,cy,cz; // start of current chunk
	int chunkSize = 128;
	
	for(int xr=0; xr<SHEET_SIZE; xr++)
	for(int yr=0; yr<SHEET_SIZE; yr++)
	  if(fscanf(f,"%f,%f,%f\n",&xo,&yo,&zo)==3)
	  {
		x = (int)xo;
		y = (int)yo+4;
		z = (int)zo;

		z+=2432-256;
		y+=2304;
		x+=4096;
		if (!(x==0 && y==0 && z==0))
		{
          // Look up the point in the zarr and plot it		
          if (currentChunk && x>=cx && x<cx+chunkSize && y>=cy && y<cy+chunkSize && z>=cz && z<cz+chunkSize)
	  	  {
 		    rendered[yr][xr] = vs_chunk_get(currentChunk,z-cz,y-cy,x-cx);
		  }
		  else
		  {
			if (currentChunk)
			{
			  vs_chunk_free(currentChunk);
			}
			printf("Need to get %d,%d,%d\n",x,y,z);
			cz = z-z%128; cy = y-y%128; cx = x-x%128;
            int vol_start[3] = {cz,cy,cx};
            int chunk_dims[3] = {chunkSize,chunkSize,chunkSize};
            currentChunk = vs_vol_get_chunk(scroll_vol, vol_start,chunk_dims);

		    rendered[yr][xr] = vs_chunk_get(currentChunk,z-cz,y-cy,x-cx);
		  }
		}
	  }
    
    	
	fclose(f);   
		
	if (1==1)
	{
		char tiffName[FILENAME_LENGTH];
		strcpy(tiffName,fname);
		/* Now write tiff file */
		int l = strlen(tiffName);
		if (strcmp(tiffName+l-4,".csv"))
		  return;
		
		sprintf(tiffName+l-4,".tif");

		TIFF *tif = TIFFOpen(tiffName,"w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, SHEET_SIZE); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, SHEET_SIZE); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(SHEET_SIZE);
			for(row=0; row<SHEET_SIZE;row++)
			{
				for(int i=0; i<SHEET_SIZE; i++)
				{
				   ((uint8_t *)buf)[i] = rendered[row][i];
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: render_from_zarr <csv file>\n");
		return -1;
	}
	
	render(argv[1]);
	
	return 0;
}

