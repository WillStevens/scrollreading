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

#define SIZE 512

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;


/* This is the image */ /* 128 Mb */
unsigned char volume[SIZE][SIZE][SIZE];

unsigned char rendered[SIZE][SIZE];

void setFoldersAndZOffset(char *s, char *t, unsigned z)
{
	strncpy(imageDir,s,FOLDERNAME_LENGTH-1);
	zOffset = z;
	strncpy(targetDir,t,FOLDERNAME_LENGTH-1);
}

void loadTiffs(void)
{
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

void render(char *d,char *v, int renderOffset)
{
    char fname[FILENAME_LENGTH];
	int x,y,z;
		
	for(int y=0; y<SIZE; y++)
	for(int x=0; x<SIZE; x++)
		rendered[y][x] = 0;

    sprintf(fname,"%s/%s",d,v);
	
	printf("%s\n",fname);
	
	int points = 0;
	
    FILE *f = fopen(fname,"r");

	while(fscanf(f,"%d,%d,%d\n",&x,&y,&z)==3)
	{
		if (x<0) x=0;
		if (x>=SIZE) x=SIZE-1;
		if (z<0) z=0;
		if (z>=SIZE) z=SIZE-1;
		
		int yr = y+renderOffset;
		if (yr>=SIZE)
			yr = SIZE-1;
		if (yr<0)
			yr = 0;
		
		if (rendered[z][x] == 0)
		{
		  rendered[z][x] = volume[z][yr][x];
		  points++;
		}
//		printf("%d,%d,%d\n",z,x,rendered[z][x]);
	}
	
	fclose(f);   

	if (1==1)
	{
		/* Now write tiff file */
		int l = strlen(fname);
		if (strcmp(fname+l-4,".csv"))
		  return;
		
		strcpy(fname+l-4,".tif");

		printf("%s\n",fname);

		TIFF *tif = TIFFOpen(fname,"w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, SIZE); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, SIZE); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(SIZE);
			for(row=0;row<SIZE;row++)
			{
				for(int i=0; i<SIZE; i++)
				{
				   ((uint8_t *)buf)[i] = rendered[row][i];
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

	}
	
	/*
    sprintf(fname,"../construct/s%s/%s_%d.pgm",folderSuffix,folderSuffix,fill);

	f = fopen(fname,"w");
	fprintf(f,"P2\n512 512 255\n");

    for(int y=0; y<SIZE; y++)
	{
      for(int x=0; x<SIZE; x++)
        fprintf(f,"%d ",rendered[y][x]);
	
	  fprintf(f,"\n");
	}
	
    fclose(f);
    */	
}

int main(int argc, char *argv[])
{
	if (argc != 3 && argc != 4)
	{
		printf("Usage: render <image directory> <target directory> [offset (default=6)]\n");
		return -1;
	}

	int renderOffset = 6;
	
	if (argc==4)
	  renderOffset = atoi(argv[3]);
  
	// assume that the last 5 digits of the image directory are the z-offset
	int zOffset = 0;
	
	if (strlen(argv[1])>=5)
	  zOffset = atoi(argv[1]+strlen(argv[1])-5);
	
	printf("zOffset=%d\n",zOffset);
	
    setFoldersAndZOffset(argv[1],argv[2],zOffset);  
    loadTiffs();

	{
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir (argv[2])) != NULL) {
			/* print all the files and directories within directory */
			while ((ent = readdir (dir)) != NULL) {
				printf ("%s\n", ent->d_name);
				
				if (ent->d_name[0]=='v')
				{
					render(argv[2],ent->d_name,renderOffset);
				}
			}
			closedir (dir);
		} else {
			/* could not open directory */
			printf("Could not open directory\n");
			return -2;
		}
	}
}

