#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#define SIZE 512
#define RENDER_SIZE 2048

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

uint8_t image[SIZE][SIZE];
uint8_t interpolated[SIZE][SIZE];

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

void Interpolate(int x, int y, int *xi, int *yi, int *zi)
{
	static int debug = 0;
	
	*xi = -1;
	*yi = -1;
	*zi = -1;
	
	int yminus = -1;
	int yplus = -1;
	int xminus = -1;
	int xplus = -1;
	
	// Look for first non-zero point in -y direction
	for(int yo = y; yo>=0; yo--)
	  if (image[yo][x] != 0)
	  {
		  yminus = yo;
		  break;
	  }
	// Look for first non-zero point in +y direction
	for(int yo = y; yo<ySize; yo++)
	  if (image[yo][x] != 0)
	  {
		  yplus = yo;
		  break;
	  }
	// Look for first non-zero point in -x direction
	for(int xo = x; xo>=0; xo--)
	  if (image[y][xo] != 0)
	  {
		  xminus = xo;
		  break;
	  }
	// Look for first non-zero point in +x direction
	for(int xo = x; xo<xSize; xo++)
	  if (image[y][xo] != 0)
	  {
		  xplus = xo;
		  break;
	  }
	
	if (xminus>=0 && xplus>=0)
	{
		int wminus = (1000*(x-xminus))/(xplus - xminus);
		int wplus = 1000-wminus;
		
		*xi = (renderPoint[y][xminus][0]*wminus + renderPoint[y][xplus][0]*wplus)/1000;
		*yi = (renderPoint[y][xminus][1]*wminus + renderPoint[y][xplus][1]*wplus)/1000;
		*zi = (renderPoint[y][xminus][2]*wminus + renderPoint[y][xplus][2]*wplus)/1000;		
	}
    else if (yminus>=0 && yplus>=0 && yplus-yminus <= 64)
	{
		int wminus = (1000*(y-yminus))/(yplus - yminus);
		int wplus = 1000-wminus;
		
		*xi = (renderPoint[yminus][x][0]*wminus + renderPoint[yplus][x][0]*wplus)/1000;
		*yi = (renderPoint[yminus][x][1]*wminus + renderPoint[yplus][x][1]*wplus)/1000;
		*zi = (renderPoint[yminus][x][2]*wminus + renderPoint[yplus][x][2]*wplus)/1000;
	}		
}

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		printf("Usage: interpolate <image> <projected points> <target>\n");
		return -1;
	}

	printf("Loading image...\n");
	loadTiff(argv[1]);

	printf("xSize=%d, ySize=%d\n",xSize,ySize);
	
	printf("Loading projected points array...\n");

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

	printf("Loaded %d projected points\n",pointCount);
	
	printf("Interpolating...\n");

	f = fopen(argv[3],"w");
	
	if (f)
	{
		int xi,yi,zi;
		
		printf("Beginning...\n");
		for(int y = 0; y<ySize; y++)
		for(int x = 0; x<xSize; x++)
		{
			if (image[y][x]==0)
			{
				Interpolate(x,y,&xi,&yi,&zi);
				
				if (xi != -1)
				  fprintf(f,"%d,%d,%d\n",xi,yi,zi);
			}
		}
	}

	fclose(f);
	
	return 0;
}




