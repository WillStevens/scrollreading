/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <vector>
#include <string>

#include "tiffio.h"

#include "zarr_show2_u8.h"

void ZarrShow2U8(ZARR_1_b700 *za, int xcoord, int ycoord, int zcoord, int width, int height, const std::string &tifName, std::vector<Patch *> patches, int bgr, int bgg, int bgb)
{
    unsigned char *pointBuffer;

	printf("Allocating...\n");
	
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
	    if (p->ContainsZ(zcoord))\
		{\
		    p->Interpolate();\
			for(patchPoint &pt : *(p->interpolatedPoints))\
			{\
				if ((int)pt.v.z == zcoord && (int)pt.v.x>=xcoord && (int)pt.v.x<xcoord+width && (int)pt.v.y>=ycoord && (int)pt.v.y<ycoord+height)\
				{\
					for(int xo=-1; xo<=1; xo++)\
					for(int yo=-1; yo<=1; yo++)\
						if ((int)pt.v.x>=xcoord-xo && (int)pt.v.x<xcoord+width-xo && (int)pt.v.y>=ycoord-yo && (int)pt.v.y<ycoord+height-yo)\
						{\
						pointBuffer[((int)pt.v.x-xcoord+xo)+((int)pt.v.y-ycoord+yo)*width] = i;\
						}\
				}\
			}\
		}\
		\
		i++;\
	}\
	}

	printf("Adding points...\n");
	
	PATCH_RENDER_LOOP(patches,1)
			
	printf("Rendering...\n");
			
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
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
			
			buf = _TIFFmalloc(width*3);
			for(row=0; row<height;row++)
			{
				for(int i=0; i<width; i++)
				{
					unsigned char pixel = ZARRRead_1_b700(za,zcoord,ycoord+row,xcoord+i);
					
					if (pixel>0)
					{
						((uint8_t *)buf)[3*i] = pixel/4;
						((uint8_t *)buf)[3*i+1] = pixel/4;
						((uint8_t *)buf)[3*i+2] = pixel/4;
					}
					else
					{
						((uint8_t *)buf)[3*i] = bgr;
						((uint8_t *)buf)[3*i+1] = bgg;
						((uint8_t *)buf)[3*i+2] = bgb;
					}
					
                   //printf("%d ",(int)pixel);				   
				   //overwrite if pointbuffer has something
				   if (pointBuffer[i+row*width] != 0)
				   {
					   int ci = pointBuffer[i+row*width]-1;
				       ((uint8_t *)buf)[3*i] = 255-80*(ci%3)-(ci/105)%79;
				       ((uint8_t *)buf)[3*i+1] = 255-40*((3*ci)%5)-(ci/105)%37;
				       ((uint8_t *)buf)[3*i+2] = 255-26*((5*ci)%7)-(ci/105)%23;
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
	
	printf("Done\n");

}

