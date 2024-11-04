#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#define SIZE 512
#define RENDER_SIZE 2048

uint8_t image[SIZE][SIZE];


int renderPoint[RENDER_SIZE][RENDER_SIZE][3];

int ySize, xSize;

void loadTiff(const char *fname)
{
        TIFF *tif = TIFFOpen(fname,"r");
    
        if (tif)
        {
            uint32_t imagelength;
            tdata_t buf;
            uint32_t row;
            TIFFGetField(tif,TIFFTAG_IMAGELENGTH,&imagelength);
			
			ySize = imagelength;
			
            uint32_t linesize = TIFFScanlineSize(tif);
			
			xSize = linesize;
			
            buf = _TIFFmalloc(linesize);
            for(row=0;row<(imagelength<SIZE?imagelength:SIZE);row++)
            {
                TIFFReadScanline(tif,buf,row,0);
                for(int i=0; i<(linesize<SIZE?linesize:SIZE); i++)
                {
                    uint8_t v1 = ((uint8_t *)buf)[i];
                    image[row][i] = v1;
					                }
            }
            _TIFFfree(buf);
        }
    
        TIFFClose(tif);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: bumpy <image> <projected points>\n");
		return -1;
	}

//	printf("Loading image...\n");
	loadTiff(argv[1]);

//	printf("xSize=%d, ySize=%d\n",xSize,ySize);

//	printf("Loading projected points array...\n");

	FILE *f = fopen(argv[2],"r");
	
	int pointCount = 0;
	int done = 0;
	while(f && !done)
	{
		int xr,yr,zr;
		
		for(int y = 0; y<ySize && !done; y++)
		for(int x = 0; x<xSize && !done; x++)
		{
			if (fscanf(f,"%d,%d,%d",&xr,&yr,&zr)==3)
			{
				renderPoint[y][x][0] = xr; 
				renderPoint[y][x][1] = yr; 
				renderPoint[y][x][2] = zr; 
			}
			else
				done = 1;
			
			pointCount++;
		}
	}
	
	fclose(f);

//	printf("Loaded %d projected points\n",pointCount);

    printf("P2\n# image.pgm\n");
	printf("%d %d\n",xSize-2,ySize-2);
	printf("256\n");
	
	for(int y = 1; y<ySize-1; y++)
	{
	for(int x = 1; x<xSize-1; x++)
	{
		if (renderPoint[y][x][0] != -1)
		{
			float xr=0,yr=0,zr=0;
			int tot = 0;
		
			for(int xo=-1; xo<=1; xo++)
			for(int yo=-1; yo<=1; yo++)
			{
				if (renderPoint[y+yo][x+xo][0] != -1)
				{
					xr += renderPoint[y+yo][x+xo][0]; 
					yr += renderPoint[y+yo][x+xo][1]; 
					zr += renderPoint[y+yo][x+xo][2];
					tot++;
				}
			}
		
			xr /= tot; yr /= tot; zr /= tot;
		
			float xd = xr - renderPoint[y][x][0];
			float yd = yr - renderPoint[y][x][1];
			float zd = zr - renderPoint[y][x][2];
		
			float m = xd*xd+yd*yd+zd*zd;
		
			int d = (m<1) ? m*256 : 256;
			
			printf("%d ",d);
		}
		else
			printf("0 ");
	}
	printf("\n");
	}
}