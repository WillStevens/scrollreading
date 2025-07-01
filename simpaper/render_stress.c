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

#define SHEET_SIZE 500

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

unsigned char rendered[SHEET_SIZE][SHEET_SIZE];

void render(char *fname)
{
	float s;
		
	FILE *f = fopen(fname,"r");
	
	for(int y=0; y<SHEET_SIZE; y++)
	for(int x=0; x<SHEET_SIZE; x++)
		rendered[y][x] = 255;

	for(int xr=0; xr<SHEET_SIZE; xr++)
	for(int yr=0; yr<SHEET_SIZE; yr++)
	  if(fscanf(f,"%f\n",&s)==1)
	  {
		// s = s *20000; // 20000 when max stress is 0.065
	    //s = s *800000; // 800000 when max stress is 0.00035
		 s = s *5000; // 20000 when max stress is 0.065
		if (s>255) s=255;
		if (s==0) s=255;
	    rendered[yr][xr] = (int)s;
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
		printf("Usage: render_stress <csv file>\n");
		return -1;
	}
	
	render(argv[1]);
	
	return 0;
}

