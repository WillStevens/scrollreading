/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

#include "tiffio.h"

#include "zarr_show2_u8.h"

bool LoadOrCreateTiffIntoBuffer(ZARR_1_b700 *za, uint32_t *pointBuffer, int xcoord, int ycoord, int zcoord, int width, int height, const std::string &tifName)
{
	TIFF *tif = TIFFOpen(tifName.c_str(), "r");
    if (!tif) 
	{
        printf("Failed to open TIFF file, creating...\n");
		tif = TIFFOpen(tifName.c_str(),"w");
		
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
			unsigned char *zarrPixels = (unsigned char *)malloc(width);
			
			for(row=0; row<height;row++)
			{
				int n = (128-xcoord%128);
				if (width<n) n = width;
				int curx = xcoord;
				
				while(curx<xcoord+width)
				{
					ZARRReadN_1_b700(za,zcoord,ycoord+row,curx,n,zarrPixels+curx-xcoord);
					curx += n;
					n=xcoord+width-curx;
					if (n>128) n=128;
				}
									
				for(int i=0; i<width; i++)
				{
					//unsigned char pixel = ZARRRead_1_b700(za,zcoord,ycoord+row,xcoord+i);
					unsigned char pixel = zarrPixels[i];
					
					if (pixel>0)
					{
						((uint8_t *)buf)[3*i] = pixel;
						((uint8_t *)buf)[3*i+1] = pixel;
						((uint8_t *)buf)[3*i+2] = pixel;
						pointBuffer[i+row*width]=(uint32_t)-1; 
					}
					else
					{
						((uint8_t *)buf)[3*i] = 0;
						((uint8_t *)buf)[3*i+1] = 0;
						((uint8_t *)buf)[3*i+2] = 0;
					    pointBuffer[i+row*width]=0; 
					}

				}
				//printf("\n");
				TIFFWriteScanline(tif,buf,row,0);
			}
			
			free(zarrPixels);
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);
	}
	else
	{
		uint32_t tif_width, tif_height;
		uint16_t samplesPerPixel, bitsPerSample;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &tif_width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &tif_height);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

		if (width != tif_width || height != tif_height)
		{
			printf("Width and height mismatch: tif=%u,%u, expected=%u,%u\n", tif_width,tif_height,width, height);
			TIFFClose(tif);
			return false;
		}

		tsize_t scanlineSize = TIFFScanlineSize(tif);
		unsigned char *buf = (unsigned char *)_TIFFmalloc(scanlineSize);

		for (uint32_t row = 0; row < height; row++)
		{
			if (TIFFReadScanline(tif, buf, row, 0) < 0) {
				printf("Error reading scanline %u\n", row);
				break;
			}

			for(int i=0; i<width; i++)
			{
				pointBuffer[i+row*width]=(buf[3*i]?(uint32_t)-1:0); 
			}
		}

		_TIFFfree(buf);
		TIFFClose(tif);
    }
	
	return true;
}
	
void ZarrShow2U8(ZARR_1_b700 *za, int xcoord, int ycoord, int zcoord, int width, int height, const std::string &tifName, std::vector<Patch *> patches, std::set<Patch *> &shown, int bgr, int bgg, int bgb, bool showGlobalCoord)
{
    uint32_t *pointBuffer;

	printf("Show global coord:%s\n",showGlobalCoord?"true":"false");
	printf("Allocating...\n");
	
	pointBuffer = (uint32_t *)malloc(width*height*sizeof(uint32_t));
	
	if (!pointBuffer)
	{
		printf("Unable to allocate memory for pointBuffer");
		exit(-1);
	}

	{
		std::stringstream oss;
		oss << OUTPUT_DIR << "/tifbuf/t_" << std::setfill('0') << std::setw(8) << zcoord << ".tif";
		LoadOrCreateTiffIntoBuffer(za,pointBuffer,xcoord,ycoord,zcoord,width,height,oss.str());
	}
	
#define PATCH_RENDER_LOOP(patches,istart) \
	{\
	int i = istart;\
	for(Patch *p : patches)\
	{\
	    if (p->ContainsZ(zcoord))\
		{\
			for(patchPoint &pt : p->InterpolateAtZ(zcoord))\
			{\
				if ((int)pt.v.z == zcoord && (int)pt.v.x>=xcoord && (int)pt.v.x<xcoord+width && (int)pt.v.y>=ycoord && (int)pt.v.y<ycoord+height)\
				{\
					for(int xo=-1; xo<=1; xo++)\
					for(int yo=-1; yo<=1; yo++)\
						if ((int)pt.v.x>=xcoord-xo && (int)pt.v.x<xcoord+width-xo && (int)pt.v.y>=ycoord-yo && (int)pt.v.y<ycoord+height-yo)\
						{\
						  if (showGlobalCoord)\
						  {\
						    /* Need to convert from patch x,y coord to global x,y coord */ \
						    float gx,gy; \
							if (p->PatchXYToGlobalXY(pt.x,pt.y,gx,gy)) \
							{\
							  uint32_t bufferValue = 1000000000+gy; \
						      pointBuffer[((int)pt.v.x-xcoord+xo)+((int)pt.v.y-ycoord+yo)*width] = bufferValue; \
						      shown.insert(p);\
							}\
						  }\
						  else\
						  {\
						    pointBuffer[((int)pt.v.x-xcoord+xo)+((int)pt.v.y-ycoord+yo)*width] = p->patchNum+1;\
						    shown.insert(p);\
						  }\
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
					if (pointBuffer[i+row*width]==(uint32_t)-1)
					{
						((uint8_t *)buf)[3*i] = 255/4;
						((uint8_t *)buf)[3*i+1] = 255/4;
						((uint8_t *)buf)[3*i+2] = 255/4;
					}
					else if (pointBuffer[i+row*width] == 0)
					{
						((uint8_t *)buf)[3*i] = bgr;
						((uint8_t *)buf)[3*i+1] = bgg;
						((uint8_t *)buf)[3*i+2] = bgb;
					}
					else if (showGlobalCoord)
					{
						uint32_t ci = pointBuffer[i+row*width]-1000000000;
						
						uint32_t rm = ci%2000;
						uint32_t gm = ci%3000;
						uint32_t bm = ci%5000;
						
						if (rm>=1000) rm = 2000-rm;
						if (gm>=1500) gm = 3000-gm;
						if (bm>=2500) bm = 5000-bm;
						
						((uint8_t *)buf)[3*i] = 64+rm*191/1000;
						((uint8_t *)buf)[3*i+1] = 64+gm*191/1500;
						((uint8_t *)buf)[3*i+2] = 64+bm*191/2500;
					}
					else if (pointBuffer[i+row*width] != 0)
				    {
					    uint32_t ci = pointBuffer[i+row*width]-1;
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

