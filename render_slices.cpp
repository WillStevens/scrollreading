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

#include <unordered_set>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <utility>

#define SIZE 512
#define RENDER_SIZE 2048

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256

typedef std::vector<std::tuple<float,float,float> > pointSet;

/* This is the image */ /* 128 Mb */
unsigned char volume[SIZE][SIZE][SIZE];

unsigned char rendered[RENDER_SIZE][RENDER_SIZE];

// Two vectors that define the projection plane
// The third vector in this array is the normal to the projection plane, calculated from the other two
int32_t planeVectors[3][3] = 
	{ { 1, 0, 0 },
	  { 0, 0, 1 } };


void LoadTiffs(const char *imageDir)
{
	// assume that the last 5 digits of the image directory are the z-offset
	int zOffset = 0;
	
	if (strlen(imageDir)>=5)
	  zOffset = atoi(imageDir+strlen(imageDir)-5);
	
    for(uint32_t m = 0; m<SIZE; m++)
    {
        char fname[FILENAME_LENGTH];
        sprintf(fname,"%s/%05d.tif",imageDir,zOffset+m);
        TIFF *tif = TIFFOpen(fname,"r");
    
        if (tif)
        {
            uint32_t imagelength;
            tdata_t buf;
            uint32_t row;
            TIFFGetField(tif,TIFFTAG_IMAGELENGTH,&imagelength);
            uint32_t linesize = TIFFScanlineSize(tif);
            buf = _TIFFmalloc(linesize);
            for(row=0;row<(imagelength<SIZE?imagelength:SIZE);row++)
            {
                TIFFReadScanline(tif,buf,row,0);
                for(int i=0; i<SIZE; i++)
                {
                    uint16_t v1 = ((uint16_t *)buf)[i];
                    volume[m][row][i] = v1/256;
                }
            }
            _TIFFfree(buf);
        }
    
        TIFFClose(tif);
    }
}

int32_t projectN(int x, int y, int z, int n)
{
	int32_t r = (x*planeVectors[n][0]+y*planeVectors[n][1]+z*planeVectors[n][2])/1000;
	
	return r;
}

pointSet LoadVolume(const char *file)
{
	float x,y,z;
	
    pointSet volume;
	
	FILE *f = fopen(file,"r");
	
	while(f)
	{
		if (fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
		{
		  volume.push_back( std::tuple<float,float,float>(x,y,z) );		  
		}
		else
			break;
	}
	
	fclose(f);
	
	return std::move(volume);
}

void Render(pointSet flat, pointSet deformed, const char *outputFile)
{
	int render_minx=RENDER_SIZE-1,render_miny=RENDER_SIZE-1,render_maxx=0,render_maxy=0;	
		
	for(int y=0; y<RENDER_SIZE; y++)
	for(int x=0; x<RENDER_SIZE; x++)
		rendered[y][x] = 0;

    for(int i = 0; i<flat.size(); i++)
	{
		float xf = std::get<0>(flat[i]);
		float yf = std::get<1>(flat[i]);
		float zf = std::get<2>(flat[i]);

		float xd = std::get<0>(deformed[i]);
		float yd = std::get<1>(deformed[i]);
		float zd = std::get<2>(deformed[i]);
		
		if (i%10000==0)
		{
			printf("%f,%f,%f : %f,%f,%f\n",xf,yf,zf,xd,yd,zd);
		}
		
		if (xf<render_minx) render_minx = xf;
		if (xf>render_maxx) render_maxx = xf;
		if (zf<render_miny) render_miny = zf;
		if (zf>render_maxy) render_maxy = zf;

		//xd += 0.192-1.43;
		//yd = (156.0-7.5)-yd;
		//zd += 0.239-1.11;

	    yd=yd+12; if (yd<0) yd=0;
		
		if (xf>=0 && zf>=0 && xf<RENDER_SIZE && zf<RENDER_SIZE && zd>=0 && yd>=0 && xd>=0 && zd<SIZE && yd<SIZE && xd<SIZE)
			rendered[(int)zf][(int)xf] = volume[(int)zd][(int)yd][(int)xd];
	}
	
	printf("Saving image...\n");
	if (1==1)
	{
		TIFF *tif = TIFFOpen(outputFile,"w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, render_maxx-render_minx+1); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, render_maxy-render_miny+1); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(render_maxx-render_minx+1);
			for(row=render_miny;row<=render_maxy;row++)
			{
				for(int i=render_minx; i<=render_maxx; i++)
				{
				   ((uint8_t *)buf)[i-render_minx] = rendered[row][i];
				}
				TIFFWriteScanline(tif,buf,row-render_miny,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

	}
}

int main(int argc, char *argv[])
{
	if (argc != 5)
	{
		/* points in deformed CSV file are in the same order as surface csv file */
		/* write a target image */
		printf("Usage: render_slices <image directory> <flat surface csv file> <deformed csv file> <target file>\n");
		return -1;
	}
      
	printf("Loading tiffs...\n");
	LoadTiffs(argv[1]);

	printf("Loading volumes...\n");
	pointSet flat = LoadVolume(argv[2]);
	pointSet deformed = LoadVolume(argv[3]);
	
	if (flat.size() != deformed.size())
	{
		printf("Size mismatch between flat and deformed point sets\n");
		exit(1);
	}
	
	printf("Rendering...\n");
	Render(flat,deformed,argv[4]);
	
    return 0;
}

