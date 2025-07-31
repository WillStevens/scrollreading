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

#include "zarr_c32i1b64.c"
#include "../simpaper/bigpatch1.cpp"

ZARR_c32i1b64 *za;

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
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(width);
			for(row=0; row<height;row++)
			{
				for(int i=0; i<width; i++)
				{
				    ZARRReadN_c32i1b64(za,zcoord,ycoord+row,xcoord+i,0,3,vfi);
					unsigned char pixel = sqrt(vfi[0]*vfi[0]+vfi[1]*vfi[1]+vfi[2]*vfi[2]);
				   ((uint8_t *)buf)[i] = pixel;
				   
				   //overwrite if pointbuffer has something
				   if (pointBuffer[i+row*width] == 255)
				       ((uint8_t *)buf)[i] = 255;
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
	if (argc != 8 && argc != 9)
	{
		printf("Usage: zarr_show <zarr file> <x> <y> <z> <w> <h> <tiff file> [pointset (.csv or .bp)]\n");
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
	
	if (argc == 9 && strlen(argv[8])>4 && !strcmp(argv[8]+strlen(argv[8])-4,".csv"))
	{
		FILE *f = fopen(argv[8],"r");
		float x,y,z;
		
		while (fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
		{
			if ((int)z == zcoord && (int)x>=xcoord && (int)x<xcoord+width && (int)y>=ycoord && (int)y<ycoord+height)
			{
//				printf("Added %d,%d to pointBuffer\n",((int)x-xcoord),((int)y-ycoord));
				pointBuffer[((int)x-xcoord)+((int)y-ycoord)*width] = 255;
			}
		}
		
		fclose(f);
	}
	else if (argc == 9 && strlen(argv[8])>3 && !strcmp(argv[8]+strlen(argv[8])-3,".bp"))
	{
		printf("Opening bigpatch %s\n",argv[8]);
		
		BigPatch *bp = NULL;
		std::vector<chunkIndex> bpChunks;
	
		bp = OpenBigPatch(argv[8]);
		printf("A\n");
		bpChunks = GetAllPatchChunks(bp);
		printf("B\n");
	
		for(auto const &i : bpChunks)
		{
            if (std::get<2>(i) != zcoord/PATCH_CHUNK_SIZE)
				continue;
			printf("C\n");
			
			std::vector<gridPoint> gridPoints;
			ReadPatchPoints(bp,i,gridPoints);
			
			for(auto const &gp : gridPoints)
			{
				int x = (int)std::get<2>(gp);
				int y = (int)std::get<3>(gp);
				int z = (int)std::get<4>(gp);
				
				if ((int)z == zcoord && (int)x>=xcoord && (int)x<xcoord+width && (int)y>=ycoord && (int)y<ycoord+height)
				{
//				printf("Added %d,%d to pointBuffer\n",((int)x-xcoord),((int)y-ycoord));
					pointBuffer[((int)x-xcoord)+((int)y-ycoord)*width] = 255;
				}
			}
		}
		
		CloseBigPatch(bp);
	}
	
	za = ZARROpen_c32i1b64(argv[1]);
	
	render(argv[7]);
	
	ZARRClose_c32i1b64(za);
	
	return 0;
}

