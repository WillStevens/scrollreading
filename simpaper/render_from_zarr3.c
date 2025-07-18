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

#include "../zarrlike/zarr_1.c"

#define ROUND(x) ((x)-(int)(x)<0.5?(int)(x):((int)(x))+1)

#define SHEET_SIZE_X 3000
#define SHEET_SIZE_Y 6000

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

unsigned char rendered[SHEET_SIZE_Y][SHEET_SIZE_X];

void render(char *fname, int maskOnly)
{
	float xo,yo,zo;
	int x,y,z;
		
	for(int y=0; y<SHEET_SIZE_Y; y++)
	for(int x=0; x<SHEET_SIZE_X; x++)
		rendered[y][x] = 0;

    //initialize the volume
    ZARR_1 *volumeZarr;

    if (!maskOnly)
        volumeZarr = ZARROpen_1("d:/zarrs/54keV_7.91um_Scroll1A.zarr/0/");
	
    FILE *f = fopen(fname,"r");
		
	float xr,yr;
	int i = 0;
	while(fscanf(f,"%f,%f,%f,%f,%f\n",&xr,&yr,&xo,&yo,&zo)==5)
	{
		x = (int)xo;
		y = (int)yo;
		z = (int)zo;

		if (1)
		{
		  
		  for(int xo=ROUND(xr)+(maskOnly?-1:0); xo<=ROUND(xr)+(maskOnly?1:0); xo++)
		  for(int yo=ROUND(yr)+(maskOnly?-1:0); yo<=ROUND(yr)+(maskOnly?1:0); yo++)
		  {
			if (xo>=0 && xo<SHEET_SIZE_X && yo>=0 && yo<SHEET_SIZE_Y)  
		      rendered[yo][xo] = maskOnly?255:ZARRRead_1(volumeZarr,z,y,x);
		  }
		}
	}
    
    	
	fclose(f);   
	
	printf("About to render\n");
	
	if (!maskOnly)
        ZARRClose_1(volumeZarr);
	
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
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, SHEET_SIZE_X); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, SHEET_SIZE_Y); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(SHEET_SIZE_X);
			for(row=0; row<SHEET_SIZE_Y;row++)
			{
				for(int i=0; i<SHEET_SIZE_X; i++)
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
	if (argc != 2 && argc != 3)
	{
		printf("Usage: render_from_zarr2 <csv file> [1 if mask only]\n");
		printf("Render from a zarr. If mak only then output a white dilated mask on black background");
		return -1;
	}
	
	render(argv[1],argc==3 && argv[2][0]=='1');
	
	return 0;
}

