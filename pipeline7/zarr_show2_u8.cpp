/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <vector>
#include <string>

#include "tiffio.h"

#include "zarr_show2_u8.h"

void ZarrShow2U8(ZARR_1 *za, int xcoord, int ycoord, int zcoord, int width, int height, const std::string &tifName, std::vector<Patch *> pNorm, std::vector<Patch *> pHighlight)
{
    unsigned char *pointBuffer;
	
	pointBuffer = (unsigned char *)malloc(width*height);
	
	if (!pointBuffer)
	{
		printf("Unable to allocate memory for pointBuffer");
		exit(-1);
	}

	memset(pointBuffer,0,width*height);

#define PATCH_RENDER_LOOP(patches,istart) \
	{\
	int i = istart;\
	for(Patch *p : patches)\
	{\
		for(patchPoint &pt : p->points)\
		{\
			if ((int)pt.v.z == zcoord && (int)pt.v.x>=xcoord && (int)pt.v.x<xcoord+width && (int)pt.v.y>=ycoord && (int)pt.v.y<ycoord+height)\
			{\
				printf("Added %d,%d to pointBuffer\n",((int)pt.v.x-xcoord),((int)pt.v.y-ycoord));\
				\
				for(int xo=-1; xo<=1; xo++)\
				for(int yo=-1; yo<=1; yo++)\
			    if ((int)pt.v.x>=xcoord-xo && (int)pt.v.x<xcoord+width-xo && (int)pt.v.y>=ycoord-yo && (int)pt.v.y<ycoord+height-yo)\
				{\
				  pointBuffer[((int)pt.v.x-xcoord+xo)+((int)pt.v.y-ycoord+yo)*width] = i;\
				}\
			}\
		}\
		\
		i++;\
	}\
	}
	
	PATCH_RENDER_LOOP(pNorm,0)
	PATCH_RENDER_LOOP(pHighlight,0x40000000)
			
	{
		TIFF *tif = TIFFOpen(tifName.c_str(),"w");
		
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
				   ((uint8_t *)buf)[3*i] = pixel/4;
				   ((uint8_t *)buf)[3*i+1] = pixel/4;
				   ((uint8_t *)buf)[3*i+2] = pixel/4;

                   //printf("%d ",(int)pixel);				   
				   //overwrite if pointbuffer has something
				   if (pointBuffer[i+row*width] != 0)
				   {
					   int ci = pointBuffer[i+row*width];
					   int bright = (ci>0x40000000)?255:191;
					   int brightDiv = (ci>0x40000000)?2:1;
				       ((uint8_t *)buf)[3*i] = bright-(64*(ci%3))/brightDiv;
				       ((uint8_t *)buf)[3*i+1] = bright-(25*((3*ci)%5))/brightDiv;
				       ((uint8_t *)buf)[3*i+2] = bright-(18*((5*ci)%7))/brightDiv;
				   }
				}
				//printf("\n");
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);
	}
	
	free(pointBuffer);
}

