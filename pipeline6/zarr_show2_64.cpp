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

#include "zarr_c64i1b64.c"
#include "bigpatch.cpp"

int badPatches[] = {  279,668,838,
676, 386, 351, 606, 460, 377, 597, 395, 589, 532, 545, 37, 734, 523, 282, 435, 389, 522, 651, 917, 832, 505, 381, 405, 907, 620, 345, 531, 308, 578,

553, 989, 536, 208, 581, 739, 25, 181, 541, 63, 252, 990, 1027, 65, 397, 773, 16, 259, 586, 684, 533, 759, 753, 355, 122, 38, 310, 540, 200, 879,

8, 76, 574, 463, 485, 0, 69, 30, 201, 1, 117, 584, 735, 847, 9, 183, 518, 338, 145, 154, 107, 175, 80, 517, 111, 352, 20, 4, 58, 3,

-1};

bool IsBadPatch(int p)
{
	for(int i = 0; badPatches[i] != -1; i++)
	{
		if (p==badPatches[i]) return true;
	}
	
	return false;
}


typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

ZARR_c64i1b64 *za;

int xcoord,ycoord,zcoord,width,height;

unsigned char *pointBuffer;

void render(char *fname)
{		
    int8_t vfi[4];
	
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
				    ZARRReadN_c64i1b64(za,zcoord,ycoord+row,xcoord+i,0,4,vfi);
					//unsigned char pixel = sqrt(vfi[0]*vfi[0]+vfi[1]*vfi[1]+vfi[2]*vfi[2]);
				   //((uint8_t *)buf)[i] = pixel;
				   ((uint8_t *)buf)[3*i] = (vfi[0]+128)*2/3;
				   ((uint8_t *)buf)[3*i+1] = (vfi[1]+128)*2/3;
				   ((uint8_t *)buf)[3*i+2] = (vfi[2]+128)*2/3;
				   
				   //overwrite if pointbuffer has something
				   if (pointBuffer[i+row*width] != 0)
				   {
					   int ci = pointBuffer[i+row*width];
				       ((uint8_t *)buf)[3*i] = 255-80*(ci%3);
				       ((uint8_t *)buf)[3*i+1] = 255-40*((3*ci)%5);
				       ((uint8_t *)buf)[3*i+2] = 255-26*((5*ci)%7);

				       //((uint8_t *)buf)[3*i] = 255;
				       //((uint8_t *)buf)[3*i+1] = 255;
				       //((uint8_t *)buf)[3*i+2] = 255;

				   }
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
	if (argc < 8)
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
	
	if (argc>=9)
	{
	  for(int i = 8; i<argc; i++)
	  {
		if (strlen(argv[i])>4 && !strcmp(argv[i]+strlen(argv[i])-4,".csv"))
		{
			FILE *f = fopen(argv[i],"r");
			float x,y,z;
			
			while (fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
			{
				if ((int)z == zcoord && (int)x>=xcoord && (int)x<xcoord+width && (int)y>=ycoord && (int)y<ycoord+height)
				{
	//				printf("Added %d,%d to pointBuffer\n",((int)x-xcoord),((int)y-ycoord));
					pointBuffer[((int)x-xcoord)+((int)y-ycoord)*width] = i-7;
				}
			}
			
			fclose(f);
		}
		else if (strlen(argv[i])>4 && !strcmp(argv[i]+strlen(argv[i])-4,".bin"))
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
				printf("Failed to open %s\n",argv[8]);
		}
		else if (strlen(argv[i])>3 && !strcmp(argv[i]+strlen(argv[i])-3,".bp"))
		{
			printf("Opening bigpatch %s\n",argv[i]);
			
			BigPatch *bp = NULL;
			std::vector<chunkIndex> bpChunks;
		
			bp = OpenBigPatch(argv[i]);
			printf("A\n");
			bpChunks = GetAllPatchChunks(bp);
			printf("B\n");
		
			for(auto const &ii : bpChunks)
			{
				if (std::get<2>(ii) != zcoord/PATCH_CHUNK_SIZE)
					continue;
				printf("C\n");
				
				std::vector<gridPoint> gridPoints;
				ReadPatchPoints(bp,ii,gridPoints);
				
				for(auto const &gp : gridPoints)
				{
					int x = (int)std::get<2>(gp);
					int y = (int)std::get<3>(gp);
					int z = (int)std::get<4>(gp);
					int patch = (int)std::get<5>(gp);
					if ((int)z == zcoord && !IsBadPatch(patch))
					{
					  // Thicken the line
					  for(int xo=-1; xo<=1; xo++)
					  for(int yo=-1; yo<=1; yo++)
					  if ((int)x>=xcoord-xo && (int)x<xcoord+width-xo && (int)y>=ycoord-yo && (int)y<ycoord+height-yo)
					  {
					    pointBuffer[((int)x-xcoord+xo)+((int)y-ycoord+yo)*width] = patch+1;
					  }
					}
				}
			}
			
			CloseBigPatch(bp);
		}
		  
	  }
	}

	
	za = ZARROpen_c64i1b64(argv[1]);
	
	render(argv[7]);
	
	ZARRClose_c64i1b64(za);
	
	return 0;
}

