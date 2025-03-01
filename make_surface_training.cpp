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
#define RENDER_SIZE 65

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256

typedef std::vector<std::tuple<float,float,float> > pointSet;

/* This is the image */ /* 128 Mb */
unsigned char volume[SIZE][SIZE][SIZE];

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

bool ExportImageStack(std::tuple<float,float,float> &p, const char *targetFilePrefix, int fileNumber, bool xFlip)
{
	int x = (int)std::get<0>(p);
	int y = (int)std::get<1>(p);
	int z = (int)std::get<2>(p);

	if (!(x>RENDER_SIZE/2 && y>RENDER_SIZE/2 && z>RENDER_SIZE/2 &&
	    x<SIZE-RENDER_SIZE/2-1 && y<SIZE-RENDER_SIZE/2-1 && z<SIZE-RENDER_SIZE/2-1))
	{
		return false;
	}

	printf("Saving image...\n");

	char outputFileName[FILENAME_LENGTH];
	
	sprintf(outputFileName,"%s%05d.tif",targetFilePrefix,fileNumber);
	TIFF *tif = TIFFOpen(outputFileName,"w");
		
	if (tif)
	{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, RENDER_SIZE*RENDER_SIZE); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, RENDER_SIZE); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(RENDER_SIZE*RENDER_SIZE);

			row = 0;
		    for(int yo=y-RENDER_SIZE/2; yo<=y+RENDER_SIZE/2; yo++,row++)
		    {
				int i=0;
				for(int zo=z-RENDER_SIZE/2; zo<=z+RENDER_SIZE/2; zo++)
			    {
					for(int xo=x-RENDER_SIZE/2; xo<=x+RENDER_SIZE/2; xo++,i++)
					{
						if (xFlip)
						  ((uint8_t *)buf)[i] = volume[zo][yo][2*x-xo];
						else							
						  ((uint8_t *)buf)[i] = volume[zo][yo][xo];
					}
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
	}
	
	TIFFClose(tif);
	
	return true;
}

int main(int argc, char *argv[])
{
	if (argc != 6)
	{
		/* points in deformed CSV file are in the same order as surface csv file */
		/* write a target image */
		printf("Usage: make_surface_training <image directory> <pointset csv file> <target file prefix> <offset> <seed>\n");
		return -1;
	}
      
	printf("Loading tiffs...\n");
	LoadTiffs(argv[1]);

	printf("Loading volumes...\n");
	pointSet surface = LoadVolume(argv[2]);

	srand(atoi(argv[5]));
	
	int count = 0;
	
	std::set<int> done;
	
	while (count<2000)
	{
		int r = rand()%surface.size();
	
	    if (done.count(r)==0)
		{
			done.insert(r);
			
			if (ExportImageStack(surface[r],argv[3],count+atoi(argv[4]),false))
				count++;
			if (ExportImageStack(surface[r],argv[3],count+atoi(argv[4]),true))
				count++;
		}
	}
	
    return 0;
}

