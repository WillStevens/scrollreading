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
#define RENDER_SIZE 2048

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;


/* This is the image */ /* 128 Mb */
unsigned char volume[SIZE][SIZE][SIZE];

unsigned char rendered[RENDER_SIZE][RENDER_SIZE];

// Two vectors that define the projection plane
// The third vector in this array is the normal to the projection plane, calculated from the other two
int32_t planeVectors[3][3] = 
	{ { 1, 0, 0 },
	  { 0, 0, 1 } };

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

int32_t projectN(int x, int y, int z, int n)
{
	int32_t r = (x*planeVectors[n][0]+y*planeVectors[n][1]+z*planeVectors[n][2])/1000;
	
	return r;
}

void render(char *d,char *v, int renderOffset)
{
    char fname[FILENAME_LENGTH];
	int x,y,z;
	int render_minx=RENDER_SIZE-1,render_miny=RENDER_SIZE-1,render_maxx=0,render_maxy=0;	
		
	for(int y=0; y<RENDER_SIZE; y++)
	for(int x=0; x<RENDER_SIZE; x++)
		rendered[y][x] = 0;

    sprintf(fname,"%s/%s",d,v);
	
	printf("%s\n",fname);
	
	int points = 0;
	
    FILE *f = fopen(fname,"r");

	while(fscanf(f,"%d,%d,%d\n",&x,&y,&z)==3)
	{
		x += (planeVectors[2][0]*renderOffset)/1000;
		y += (planeVectors[2][1]*renderOffset)/1000;
		z += (planeVectors[2][2]*renderOffset)/1000;
		
		if (x<0) x=0;
		if (x>=SIZE) x=SIZE-1;
		if (z<0) z=0;
		if (z>=SIZE) z=SIZE-1;
		if (y<0) y=0;
		if (y>=SIZE) y=SIZE-1;

		int pa = projectN(x,y,z,0)+RENDER_SIZE/2;
		int pb = projectN(x,y,z,1)+RENDER_SIZE/2;
				
		if (rendered[pb][pa] == 0)
		{
		  if (pa<render_minx) render_minx = pa;
		  if (pb<render_miny) render_miny = pb;
		  if (pa>render_maxx) render_maxx = pa;
		  if (pb>render_maxy) render_maxy = pb;
		  
		  rendered[pb][pa] = volume[z][y][x];
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
	if (argc != 3 && argc != 4 && argc != 10)
	{
		printf("Usage: render <image directory> <target directory> [offset (default=6) [x1 y1 z1 x2 y2 z2]]\n");
		return -1;
	}

	int renderOffset = 6;
	
	if (argc==4 || argc==10)
	  renderOffset = atoi(argv[3]);

	if (argc==10)
	{
		for(int i = 0; i<2; i++)
		  for(int j = 0; j<3; j++)
			  planeVectors[i][j] = atoi(argv[j+i*3+3]);
	}
	
	printf("Projection plane vectors normalised to length 1000:\n");
	for(int i = 0; i<2; i++)
	{
		double magnitude = sqrt((double)(planeVectors[i][0]*planeVectors[i][0]+planeVectors[i][1]*planeVectors[i][1]+planeVectors[i][2]*planeVectors[i][2]));
		
		for(int j = 0; j<3; j++)
		{
			planeVectors[i][j] /= (magnitude/1000);
			
			printf("%d ", planeVectors[i][j]);
		}
		
		printf("\n");
	}
	
	/* Cross product of the two plane vectors gives the normal vector */
	planeVectors[2][0] = (planeVectors[0][1]*planeVectors[1][2]-planeVectors[0][2]*planeVectors[1][1])/1000;
	planeVectors[2][1] = (planeVectors[0][2]*planeVectors[1][0]-planeVectors[0][0]*planeVectors[1][2])/1000;
	planeVectors[2][2] = (planeVectors[0][0]*planeVectors[1][1]-planeVectors[0][1]*planeVectors[1][0])/1000;
	
	printf("Normal vector normalised to length 1000:\n");
	printf("%d %d %d\n",planeVectors[2][0],planeVectors[2][1],planeVectors[2][2]);
  
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

