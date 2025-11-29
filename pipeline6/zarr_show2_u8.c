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

#include "zarr_1.c"

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

ZARR_1 *za;

int xcoord,ycoord,zcoord,width,height;

unsigned char *pointBuffer;

void render(char *fname)
{		
    int8_t vfi[3];
	
	if (1==1)
	{
		TIFF *tif = TIFFOpen(fname,"w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(width*3);
			for(row=0; row<height;row++)
			{
				for(int i=0; i<width; i++)
				{
					unsigned char pixel = ZARRRead_1(za,zcoord,ycoord+row,xcoord+i);
				   ((uint8_t *)buf)[3*i] = pixel/3;
				   ((uint8_t *)buf)[3*i+1] = pixel/3;
				   ((uint8_t *)buf)[3*i+2] = pixel/3;

                   //printf("%d ",(int)pixel);				   
				   //overwrite if pointbuffer has something
				   if (pointBuffer[i+row*width] != 0)
				   {
					   int ci = pointBuffer[i+row*width];
				       ((uint8_t *)buf)[3*i] = 255-64*(ci%3);
				       ((uint8_t *)buf)[3*i+1] = 255-25*((3*ci)%5);
				       ((uint8_t *)buf)[3*i+2] = 255-18*((5*ci)%7);
				   }
				}
				//printf("\n");
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

	}
}

int main(int argc, char *argv[])
{
	if (argc < 8)
	{
		printf("Usage: zarr_show <zarr file> <x> <y> <z> <w> <h> <tiff file> [pointset]\n");
		printf("Show a slice from a zarr file, with optional points from pointset plotted (those that lie in the slice)\n");
		return -1;
	}
	
	xcoord = atoi(argv[2]);
	ycoord = atoi(argv[3]);
	zcoord = atoi(argv[4]);
	width = atoi(argv[5]);
	height = atoi(argv[6]);
	
	pointBuffer = (unsigned char *)malloc(width*height);
	memset(pointBuffer,0,width*height);
	
	if (!pointBuffer)
	{
		printf("Unable to allocate memory for pointBuffer");
		exit(-1);
	}
	
	if (argc >= 9)
	{
	  for(int i = 8; i<argc; i++)
	  {
		FILE *f = fopen(argv[i],"r");
		gridPointStruct p;

		if (f)
		{
			fseek(f,0,SEEK_END);
			long fsize = ftell(f);
			fseek(f,0,SEEK_SET);
  
			// input in x,y,z order 
			while(ftell(f)<fsize)
			{
				fread(&p,sizeof(p),1,f);
				float x,y,px,py,pz;
				x=p.x;y=p.y;px=p.px;py=p.py;pz=p.pz;
				
				if ((int)pz == zcoord && (int)px>=xcoord && (int)px<xcoord+width && (int)py>=ycoord && (int)py<ycoord+height)
				{
					printf("Added %d,%d to pointBuffer\n",((int)px-xcoord),((int)py-ycoord));
					
					// Thicken the line
					for(int xo=-1; xo<=1; xo++)
					for(int yo=-1; yo<=1; yo++)
				    if ((int)px>=xcoord-xo && (int)px<xcoord+width-xo && (int)py>=ycoord-yo && (int)py<ycoord+height-yo)
					{
					  pointBuffer[((int)px-xcoord+xo)+((int)py-ycoord+yo)*width] = i-7;
					}
				}
			}
			
			fclose(f);
		}
		else
			printf("Failed to open %s\n",argv[i]);
	  }
	}
	
	za = ZARROpen_1(argv[1]);
	
	render(argv[7]);
	
	ZARRClose_1(za);
	
	return 0;
}

