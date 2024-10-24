/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
 
 This file should be compiled to a DLL that can be loaded with python.
 
 If using cygwin then do this with e.g.:

 x86_64-w64-mingw32-gcc -shared -O3 -o scrollprocess.dll scrollprocess.c -ltiff
 
 Released under GNU Public License V3
 */


#include <stdint.h>
#include <math.h>
#include <string.h>

#include "tiffio.h"

#define PATCH_EXPORT_LIMIT 1000

#define SIZE 512

#define FOLDERNAME_LENGTH 256
#define FILENAME_LENGTH 300
char imageFolder[FOLDERNAME_LENGTH]="";
char outputFolder[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

//#define Z_OFFSET 5800 // for 005
//#define Z_OFFSET 5288 // for 105
//#define Z_OFFSET 4776 // for 205

/* This is the image */ /* 128 Mb */
uint8_t volume[SIZE][SIZE][SIZE];
uint8_t volume_ahe[SIZE][SIZE][SIZE];

/* This is a working area for processing the image */ 
uint32_t processed[SIZE][SIZE][SIZE]; 

/* Image processed by sobel edge detection */ /* 128 Mb */
uint8_t sobel[SIZE][SIZE][SIZE];

uint8_t laplace[SIZE][SIZE][SIZE]; /* 128 Mb */

/* A slice of the processed image, RGB coloured */
uint32_t processed_coloured[SIZE][SIZE];

uint8_t rendered[SIZE][SIZE];

typedef struct
{
  int x,y,z;
} vector;

vector dirVectorLookup[6] =
{
  {0,1,0},
  {1,0,0},
  {0,-1,0},
  {-1,0,0},
  {0,0,1},
  {0,0,-1},
};

// A vector that defines the direction to the scroll umbilicus
int32_t normalVector[3] = {0,-1,0};

#define FILLQUEUELENGTH (SIZE*SIZE*6*3)
uint16_t fillQueue[FILLQUEUELENGTH];
uint32_t fillQueueHead = 0;
uint32_t fillQueueTail = 0;

/* Meanings of different values in 'processed' */
/* Must match definitions in volumedll.py */
#define PR_EMPTY 0
#define PR_SCROLL 1
#define PR_PLUG 2
#define PR_TMP 3
#define PR_FILL_START 4
#define PR_FILL_END 2147483647

#define PR_FILL_OFFSET 2147483648

/* 3 - 2147483647 = filled areas 
 * 2147483648+3 - 2^32-1 = used during dilation of filled areas */

/* allow for 1000000 different fill colours */
int fillExtent[1000000][6]; /* xmin,ymin,zmin,xmax,ymax,zmax for each fill colour */
int regionVolumes[1000000];

void setFoldersAndZOffset(char *s, char *d, unsigned z)
{
	strncpy(imageFolder,s,FOLDERNAME_LENGTH-1);
	strncpy(outputFolder,d,FOLDERNAME_LENGTH-1);
	zOffset = z;
}

int getRegionVolume(int i)
{
    return regionVolumes[i];
}

int fillableVoxelBasic(int x, int y, int z)
{
	if (processed[z][y][x] != PR_SCROLL || sobel[z][y][x] != 255)
		return 0;
	
	for(int xo=-1; xo<=1; xo++)
	for(int yo=-1; yo<=1; yo++)
	for(int zo=-1; zo<=1; zo++)
        if ((xo==0 || yo==0 || zo==0) && z+zo>=0 && z+zo<SIZE && y+yo>=0 && y+yo<SIZE && x+xo>=0 && x+xo<SIZE)
        {
			if (processed[z+zo][y+yo][x+xo]==PR_EMPTY)
			{
				return 1;
			}
		}
		
    return 0;
}

/* This implements the FAFF algorithm described in report.pdf */
/* If doing DAFF then uncomment the second line of this so that it just calls fillableVoxelBasic - this will often result in large collections of layers with intersections that DAFF can be run on, to produce a list of plugs that can be pasted into scrollprocess.py, before running the pipeline a second time. (In future versions this will all be done automatically without requiring manual intervention). */

int fillableVoxel(int x, int y, int z, int v)
{
	static int rcounter = 0;
	// WS 2024-09-12 - experiment with new edge detection
	//return sobel[z][y][x]==255 && processed[z][y][x]<PR_FILL_START;
	
	return fillableVoxelBasic(x,y,z);
	
    if (!fillableVoxelBasic(x,y,z)) return 0;
    
    int cx = 0, cy = 0, cz = 0;
    int count = 0;
    
    for(int xo = -3; xo<=3; xo++)
    for(int yo = -3; yo<=3; yo++)
    for(int zo = -3; zo<=3; zo++)
    {
        int px = x+xo, py = y+yo, pz = z+zo;
        if (px<0) px = -px;
        if (px>SIZE-1) px=(SIZE-1)-(px-(SIZE-1));
        if (py<0) py = -py;
        if (py>SIZE-1) py=(SIZE-1)-(py-(SIZE-1));
        if (pz<0) pz = -pz;
        if (pz>SIZE-1) pz=(SIZE-1)-(pz-(SIZE-1));
        
        if ((processed[pz][py][px]!=PR_EMPTY && processed[pz][py][px]!=PR_SCROLL) || fillableVoxelBasic(px,py,pz))
        {
            cx += x+xo; cy += y+yo; cz += z+zo;
            count++;
        }
    }

    cx = 10*cx / count;
    cy = 10*cy / count;
    cz = 10*cz / count;
    
    int d2 = (cx-10*x)*(cx-10*x)+(cy-10*y)*(cy-10*y)+(cz-10*z)*(cz-10*z);
    
	// d2 values tried:
	// <62 too permissive
	// <30 is okay but a little permissive
	// <25 is not okay - too many small patches
	
	// d2 < 29 && count>=70 seems quite good (count was found to be more-or-less irrelevant here).
    int r = d2 < 29 && count>=70;

/* uncomment below to get fillable/unfillable datasets for training a neural network to match the behaviour of
   FAFF */	
/*	
	rcounter += 1;
	
	FILE *f = NULL;
	
	if (r && rcounter%2500==0)
	{
      char fname[100];
      sprintf(fname,"../construct/s" BLOCK_NO "/fillable.csv");
      f = fopen(fname,"a");	  
	}
	else if (!r && rcounter%5000==0)
	{
      char fname[100];
      sprintf(fname,"../construct/s" BLOCK_NO "/unfillable.csv");
      f = fopen(fname,"a");
	}

	if (f)
	{
		for(int xo = -3; xo<=3; xo++)
		for(int yo = -3; yo<=3; yo++)
		for(int zo = -3; zo<=3; zo++)
		{
			int px = x+xo, py = y+yo, pz = z+zo;
			if (px<0) px = -px;
			if (px>SIZE-1) px=(SIZE-1)-(px-(SIZE-1));
			if (py<0) py = -py;
			if (py>SIZE-1) py=(SIZE-1)-(py-(SIZE-1));
			if (pz<0) pz = -pz;
			if (pz>SIZE-1) pz=(SIZE-1)-(pz-(SIZE-1));
        
			fprintf(f,"%d,%d\n",sobel[pz][py][px]==255,processed[pz][py][px]!=PR_EMPTY);
		}
		fprintf(f,"%d\n",r);

		fclose(f);
	}
*/
	return r;
}

/* This was an experiment in using a nerual network to assess fillability */
/*
int fillableVoxelNN(int x, int y, int z, int v)
{	
    if (!fillableVoxelBasic(x,y,z)) return 0;

	static double NNinput[343*1];
	static double NNhidden[100];
	static double NNoutput[1];
	
	int i = 0;
	
	for(int xo = -3; xo<=3; xo++)
	for(int yo = -3; yo<=3; yo++)
	for(int zo = -3; zo<=3; zo++)
	{
		int px = x+xo, py = y+yo, pz = z+zo;
		if (px<0) px = -px;
		if (px>SIZE-1) px=(SIZE-1)-(px-(SIZE-1));
		if (py<0) py = -py;
		if (py>SIZE-1) py=(SIZE-1)-(py-(SIZE-1));
		if (pz<0) pz = -pz;
		if (pz>SIZE-1) pz=(SIZE-1)-(pz-(SIZE-1));
        
		NNinput[i++]=sobel[pz][py][px];
		NNinput[i++]=processed[pz][py][px];		
	}
	
#include "../neural/working/NNprocess.c"

	return NNoutput[0]>=0.5;
}
*/

// Must only be called if voxel is known to be fillable
void floodFillStart(unsigned x, unsigned y, unsigned z, unsigned v)
{
//    if (!fillableVoxel(x,y,z,v)) return;
	    
    fillQueueHead = 0;
    fillQueueTail = 0;
    fillQueue[fillQueueHead++]=x;
    fillQueue[fillQueueHead++]=y;
    fillQueue[fillQueueHead++]=z;

    fillExtent[v][0]=x;
    fillExtent[v][1]=y;
    fillExtent[v][2]=z;
    fillExtent[v][3]=x;
    fillExtent[v][4]=y;
    fillExtent[v][5]=z;
    
    if (fillQueueHead>=FILLQUEUELENGTH)
        fillQueueHead-=FILLQUEUELENGTH;
}

int findEmptyAndStartFill(unsigned v)
{
    static int x = 0, y = 0, z = 0;

    int r;
/*  
    if (x<=0 || x>=SIZE) x=0;
    if (y<=0 || y>=SIZE) y=0;
    if (z<=0 || z>=SIZE) z=0;
*/  
    while(1==1)
    {
        if (fillableVoxel(x,y,z,v))
        {
            floodFillStart(x,y,z,v);
            return 1;
        }
        
        if (++y>=SIZE)
        {
            y=0;
            if (++x>=SIZE)
            {
                x=0;
                if (++z>=SIZE)
                {
                    z=0;
                    return 0;
                }
            }
        }
    }
}

#define FILL_CASE_LO(xo,yo,zo,test) \
    if (test>=0) \
    { \
        fillQueue[fillQueueHead++]=x+xo; \
        fillQueue[fillQueueHead++]=y+yo; \
        fillQueue[fillQueueHead++]=z+zo; \
      \
        if (fillQueueHead>=FILLQUEUELENGTH) \
            fillQueueHead-=FILLQUEUELENGTH; \
    } 

#define FILL_CASE_HI(xo,yo,zo,test) \
    if (test<SIZE) \
    { \
        fillQueue[fillQueueHead++]=x+xo; \
        fillQueue[fillQueueHead++]=y+yo; \
        fillQueue[fillQueueHead++]=z+zo; \
      \
        if (fillQueueHead>=FILLQUEUELENGTH) \
            fillQueueHead-=FILLQUEUELENGTH; \
    } 

int floodFillContinue(int v)
{   
    int x,y,z;
    
    unsigned fillCount = 0;
    
    while(fillQueueHead != fillQueueTail)
    {
        x = fillQueue[fillQueueTail++];
        y = fillQueue[fillQueueTail++];
        z = fillQueue[fillQueueTail++];
        
        if (fillQueueTail>=FILLQUEUELENGTH)
            fillQueueTail-=FILLQUEUELENGTH;

        if (fillableVoxel(x,y,z,v))
        {	
        processed[z][y][x]=v;
    
        if (x<fillExtent[v][0]) fillExtent[v][0]=x;
        if (y<fillExtent[v][1]) fillExtent[v][1]=y;
        if (z<fillExtent[v][2]) fillExtent[v][2]=z;
        if (x>fillExtent[v][3]) fillExtent[v][3]=x;
        if (y>fillExtent[v][4]) fillExtent[v][4]=y;
        if (z>fillExtent[v][5]) fillExtent[v][5]=z;
    
        fillCount++;
      
		FILL_CASE_LO(-1,0,0,x-1)
		FILL_CASE_HI(1,0,0,x+1)
		FILL_CASE_LO(0,-1,0,y-1)
		FILL_CASE_HI(0,1,0,y+1)
		FILL_CASE_LO(0,0,-1,z-1)
		FILL_CASE_HI(0,0,1,z+1)			
        }

	}
    
    return fillCount;
}

/*
int floodFillContinuex(void)
{   
    unsigned x,y,z;
    
    unsigned fillCount = 0;
    
    while(fillQueueHead != fillQueueTail)
    {
        x = fillQueue[fillQueueTail++];
        y = fillQueue[fillQueueTail++];
        z = fillQueue[fillQueueTail++];
        
        if (fillQueueTail>=FILLQUEUELENGTH)
            fillQueueTail-=FILLQUEUELENGTH;
            
        if (fillableVoxel(x,y,z,fillValue))
        {
            processed[z][y][x]=fillValue;
    
            if (x<fillExtent[fillValue][0]) fillExtent[fillValue][0]=x;
            if (y<fillExtent[fillValue][1]) fillExtent[fillValue][1]=y;
            if (z<fillExtent[fillValue][2]) fillExtent[fillValue][2]=z;
            if (x>fillExtent[fillValue][3]) fillExtent[fillValue][3]=x;
            if (y>fillExtent[fillValue][4]) fillExtent[fillValue][4]=y;
            if (z>fillExtent[fillValue][5]) fillExtent[fillValue][5]=z;
    
            fillCount++;
        
            for(unsigned i = 0; i<6; i++)
            {
                vector p = dirVectorLookup[i];

                if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
                {
                    fillQueue[fillQueueHead++]=x+p.x;
                    fillQueue[fillQueueHead++]=y+p.y;
                    fillQueue[fillQueueHead++]=z+p.z;
              
                    if (fillQueueHead>=FILLQUEUELENGTH)
                        fillQueueHead-=FILLQUEUELENGTH;
                }
            }
        }
    }
    
    return fillCount;
}
*/
#define DILATE_CASE(xo,yo,zo,test) \
                if (test>=0 && test<SIZE) \
                { \
                    if (processed[z+zo][y+yo][x+xo]==PR_SCROLL) \
                    { \
                        processed[z][y][x]=PR_TMP; \
                        continue; \
                    } \
                }

void dilate(unsigned thresh)
{
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
        if (processed[z][y][x]==0 && volume[z][y][x]>=thresh)
        {
			DILATE_CASE(0,0,1,z+1)
			DILATE_CASE(0,0,-1,z-1)
			DILATE_CASE(0,1,0,y+1)
			DILATE_CASE(0,-1,0,y-1)
			DILATE_CASE(1,0,0,x+1)
			DILATE_CASE(-1,0,0,x-1)
        }

    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
        if (processed[z][y][x]==PR_TMP)
            processed[z][y][x]=PR_SCROLL;
}

void dilateFilledAreas(void)
{
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
        if (fillableVoxelBasic(x,y,z))
        {
            for(unsigned i = 0; i<6; i++)
            {
                vector p = dirVectorLookup[i];

                if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
                {
                    unsigned short v = processed[z+p.z][y+p.y][x+p.x];
                    
                    if (v >=PR_FILL_START && v<=PR_FILL_END)
                    {
                        processed[z][y][x]=v+PR_FILL_OFFSET;
                        break;
                    }
                }
            }
        }
    }
    
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
        if (processed[z][y][x] >= PR_FILL_OFFSET)
        {
            unsigned v = (processed[z][y][x] -= PR_FILL_OFFSET);
            
            if (x<fillExtent[v][0]) fillExtent[v][0]=x;
            if (y<fillExtent[v][1]) fillExtent[v][1]=y;
            if (z<fillExtent[v][2]) fillExtent[v][2]=z;
            if (x>fillExtent[v][3]) fillExtent[v][3]=x;
            if (y>fillExtent[v][4]) fillExtent[v][4]=y;
            if (z>fillExtent[v][5]) fillExtent[v][5]=z;
        }
}

void initProcessed(void)
{
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
        processed[z][y][x]=(volume[z][y][x]<=135)?PR_EMPTY:PR_SCROLL;
        
/*
        if (processed[z][y][x]==0)
        {
            int t = 1;
            int v = volume[z][y][x];
            
            for(unsigned i = 0; i<6; i++)
            {
                vector p = dirVectorLookup[i];

                if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
                {
                    t++;
                    
                    v += volume[z+p.z][y+p.y][x+p.x];
                }
            }
            
            processed[z][y][x]=(v/t<=130)?0:255;
        }
*/
}

void loadTiffs(void)
{
    for(uint32_t m = 0; m<SIZE; m++)
    {
        char fname[FILENAME_LENGTH];
        sprintf(fname,"%s/%05d.tif",imageFolder,zOffset+m);
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
    
    initProcessed();
}

unsigned char *getSlice(unsigned z)
{
    return &volume[z];
}

unsigned char *getSliceSobel(unsigned z)
{
    return &sobel[z];
}

unsigned char *getSliceLaplace(unsigned z)
{
    return &laplace[z];
}

unsigned char getVoxel(unsigned x, unsigned y, unsigned z)
{
    return volume[z][y][x];
}

int getVoxels(int x, int y, int z, unsigned char *v)
{
    *v = volume[z][y][x];
}

unsigned short *getSliceP(unsigned z)
{
    for(int x=0; x<SIZE; x++)
    for(int y=0; y<SIZE; y++)
    {
        int i = processed[z][y][x];
        
        int r=100,g=100,b=100;
        
        if (i!=1)
        {
            r = (i*127)%256;
            g = (i*83)%256;
            b = (i*53)%256;
        }
        
        processed_coloured[y][x] = (r<<0)+(b<<16)+(g<<8); 
    }
    
    return &processed_coloured;
}

unsigned char getVoxelP(unsigned x, unsigned y, unsigned z)
{
    return processed[z][y][x];
}

unsigned char setVoxel(unsigned x, unsigned y, unsigned z, unsigned v)
{
    volume[z][y][x] = v;
}

unsigned char setVoxelP(unsigned x, unsigned y, unsigned z, unsigned v)
{
    processed[z][y][x] = v;
}

unsigned char setVoxels(unsigned x, unsigned y, unsigned z, unsigned v0, unsigned v1, unsigned v2, unsigned v3, unsigned v4, unsigned v5, unsigned v6, unsigned v7)
{
    volume[z][y][x] = v0;
    volume[z][y][x+1] = v1;
    volume[z][y][x+2] = v2;
    volume[z][y][x+3] = v3;
    volume[z][y][x+4] = v4;
    volume[z][y][x+5] = v5;
    volume[z][y][x+6] = v6;
    volume[z][y][x+7] = v7;
}

unsigned char *getSliceR(unsigned z)
{
    return &rendered;
}

void render(int q, int o, int fill)
{
    char fname[FILENAME_LENGTH];
    sprintf(fname,"%s/v_%d.csv",outputFolder,fill);
    FILE *f = fopen(fname,"w");

    for(int x=0; x<SIZE; x++)
    for(int z=0; z<SIZE; z++)
        rendered[z][x]=0;

//    printf("%d\n",fill);
//    printf("%d,%d,%d,%d,%d,%d\n",
//      fillExtent[fill][0],fillExtent[fill][1],fillExtent[fill][2],fillExtent[fill][3],fillExtent[fill][4],fillExtent[fill][5]);
    
    
    for(int x=fillExtent[fill][0]; x<=fillExtent[fill][3]; x++)
    for(int z=fillExtent[fill][2]; z<=fillExtent[fill][5]; z++)
    {
//        printf("%d,%d\n",z,x);
        rendered[z][x]=0;
        int foundYet = 0;
        for (int y=fillExtent[fill][4]; y>=fillExtent[fill][1]; y--)
        {
            int v = processed[z][y][x];
            if (v==fill && !foundYet)
            {
                int t = 0;
                for (int yo=y+o; yo<y+o+q; yo++)
                {
                    if (yo<0)
                        t += volume[z][0][x];
                    else if (yo>=SIZE)
                        t += volume[z][SIZE-1][x];
                    else
                        t += volume[z][yo][x];
                }
                
//                printf("%d,%d,%d,%d\n",z,x,y,t/q);				
                rendered[z][x]=t/q;
//				printf("A\n");
                foundYet = 1;
//				printf("B\n");
            }
            if (v==fill)
            {             
              fprintf(f,"%d,%d,%d\n",x,y,z);    
            }
        }
    }
    
    fclose(f);
    
    sprintf(fname,"%s/x_%d.csv",outputFolder,fill);
    f = fopen(fname,"w");
    
    fprintf(f,"%d,%d,%d,%d,%d,%d",
      fillExtent[fill][0],fillExtent[fill][1],fillExtent[fill][2],fillExtent[fill][3],fillExtent[fill][4],fillExtent[fill][5]);
    
    fclose(f);
}

void unFill(int fill)
{
    for(int z=fillExtent[fill][2]; z<=fillExtent[fill][5]; z++)
    for(int y=fillExtent[fill][1]; y<=fillExtent[fill][4]; y++)
    for(int x=fillExtent[fill][0]; x<=fillExtent[fill][3]; x++)
    {
        if (processed[z][y][x] == fill)
            processed[z][y][x] = PR_SCROLL;
    }
}

int SobelNeighbours(int x, int y, int z)
{
    int count = 0;
    for(unsigned i = 0; i<6; i++)
    {
        vector p = dirVectorLookup[i];

         if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
        {
            if (sobel[z+p.z][y+p.y][x+p.x]==255)
            {
                count++;
            }
        }
    }
	
	return count;
}

void Sobel(void)
{
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
        int xm1 = x?x-1:x+1, ym1 = y?y-1:y+1, zm1 = z?z-1:z+1;
        int xp1 = (x<SIZE-1)?x+1:x-1, yp1 = (y<SIZE-1)?y+1:y-1, zp1 = (z<SIZE-1)?z+1:z-1;
        
//      printf("%d %d %d %d %d %d\n",xm1,ym1,zm1,xp1,yp1,zp1);
        
        int dx,dy,dz;
        
        dx = 1 * (int)volume[zm1][ym1][xm1] - 1 * (int)volume[zm1][ym1][xp1];
        dx += 2 * (int)volume[zm1][y][xm1]  - 2 * (int)volume[zm1][y][xp1];
        dx += 1 * (int)volume[zm1][yp1][xm1] - 1 * (int)volume[zm1][yp1][xp1];
        dx += 2 * (int)volume[z][ym1][xm1]  - 2 * (int)volume[z][ym1][xp1];
        dx += 4 * (int)volume[z][y][xm1] - 4 * (int)volume[z][y][xp1];
        dx += 2 * (int)volume[z][yp1][xm1] - 2 * (int)volume[z][yp1][xp1];
        dx += 1 * (int)volume[zp1][ym1][xm1] - 1 * (int)volume[zp1][ym1][xp1];
        dx += 2 * (int)volume[zp1][y][xm1] - 2 * (int)volume[zp1][y][xp1];
        dx += 1 * (int)volume[zp1][yp1][xm1] - 1 * (int)volume[zp1][yp1][xp1];

        dy = 1 * (int)volume[zm1][ym1][xm1] - 1 * (int)volume[zm1][yp1][xm1];
        dy += 2 * (int)volume[zm1][ym1][x] - 2 * (int)volume[zm1][yp1][x];
        dy += 1 * (int)volume[zm1][ym1][xp1] - 1 * (int)volume[zm1][yp1][xp1];
        dy += 2 * (int)volume[z][ym1][xm1] - 2 * (int)volume[z][yp1][xm1];
        dy += 4 * (int)volume[z][ym1][x] - 4 * (int)volume[z][yp1][x];
        dy += 2 * (int)volume[z][ym1][xp1] - 2 * (int)volume[z][yp1][xp1];
        dy += 1 * (int)volume[zp1][ym1][xm1] -1 * (int)volume[zp1][yp1][xm1];
        dy += 2 * (int)volume[zp1][ym1][x] - 2 * (int)volume[zp1][yp1][x];
        dy += 1 * (int)volume[zp1][ym1][xp1] - 1 * (int)volume[zp1][yp1][xp1];
        
        dz = 1 * (int)volume[zm1][ym1][xm1] - 1 * (int)volume[zp1][ym1][xm1];
        dz += 2 * (int)volume[zm1][ym1][x] - 2 * (int)volume[zp1][ym1][x];
        dz += 1 * (int)volume[zm1][ym1][xp1] - 1 * (int)volume[zp1][ym1][xp1];
        dz += 2 * (int)volume[zm1][y][xm1] -2 * (int)volume[zp1][y][xm1];
        dz += 4 * (int)volume[zm1][y][x] - 4 * (int)volume[zp1][y][x];
        dz += 2 * (int)volume[zm1][y][xp1] - 2 * (int)volume[zp1][y][xp1];
        dz += 1 * (int)volume[zm1][yp1][xm1] - 1 * (int)volume[zp1][yp1][xm1];
        dz += 2 * (int)volume[zm1][yp1][x] - 2 * (int)volume[zp1][yp1][x];
        dz += 1 * (int)volume[zm1][yp1][xp1] - 1 * (int)volume[zp1][yp1][xp1];
/*
        dx = (int)volume[z][y][xp1]-(int)volume[z][y][xm1]; 
        dy = (int)volume[z][yp1][x]-(int)volume[z][ym1][x]; 
        dz = (int)volume[zp1][y][x]-(int)volume[zm1][y][x]; 
*/
        /* If the dot product of the sobel vector and the direction to the scroll umbilicus is positive (i.e. the sobel points within 90
		 * degrees of direction to scroll umbilicus), and the magnitude of the sobel vector is large enough, it is a surface point */
		float dp = (float)normalVector[0]*dx + (float)normalVector[1]*dy + (float)normalVector[2]*dz;
        sobel[z][y][x] = (dp>0?sqrt(dx*dx+dy*dy+dz*dz):0)>150?255:0;
		
/*
		int laplaceInRange = laplace[z][y][x]>120 && laplace[z][y][x]<136;
		if (y>0 && !laplaceInRange)
				laplaceInRange = laplace[z][y-1][x]<=128 && laplace[z][y][x]>128;
		if (y<SIZE-1 && !laplaceInRange)
				laplaceInRange = laplace[z][y][x]<=128 && laplace[z][y+1][x]>128;

		if (x==143 && y>=320 && y<=335 && z==20)
		{
			printf("x:%d y:%d z:%d s:%d p:%d l:%d lir:%d\n",x,y,z,sobel[z][y][x],processed[z][y][x],laplace[z][y][x],laplaceInRange);
		}
			
        sobel[z][y][x] = (processed[z][y][x]==PR_SCROLL && sobel[z][y][x]==255 && laplaceInRange )?255:0;
*/
		
//        sobel_orig[z][y][x] = (dy<0?sqrt(dx*dx+dy*dy+dz*dz):0)>220?255:0;
        
    }

    /* now erode the sobel image */
    /*
    for(int x = 0; x<SIZE; x++)
    for(int y = 0; y<SIZE; y++)
    for(int z = 0; z<SIZE; z++)
        if (sobel[z][y][x]==255)
        {
            for(unsigned i = 0; i<6; i++)
            {
                vector p = dirVectorLookup[i];

                if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
                {
                    if (sobel[z+p.z][y+p.y][x+p.x]==0)
                        sobel[z][y][x]=254;
                }
            }
        }

    
    for(int x = 0; x<SIZE; x++)
    for(int y = 0; y<SIZE; y++)
    for(int z = 0; z<SIZE; z++)
        if (sobel[z][y][x]==255)
        {
            for(unsigned i = 0; i<6; i++)
            {
                vector p = dirVectorLookup[i];

                if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
                {
                    if (sobel[z+p.z][y+p.y][x+p.x]==254)
                        sobel[z][y][x]=253;
                }
            }
        }
    */
    /* finish erosion, and also make sure that only voxels that are white in processed are preserved */
    /* (processed is the dilated scroll, using it as a filter effectively removes noise in the inter-scroll voids) */
/*
    for(int x = 0; x<SIZE; x++)
    for(int y = 0; y<SIZE; y++)
    for(int z = 0; z<SIZE; z++)
        if (sobel[z][y][x]==254 || sobel[z][y][x]==253 || processed[z][y][x]==PR_SCROLL)
            sobel[z][y][x]=0;
*/

    /* get rid of any isolated points or points with neighbours that don't connect to any other point */

    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
        if (sobel[z][y][x]==255)
        {
            int found = 0;
            for(unsigned i = 0; i<6; i++)
            {
                vector p = dirVectorLookup[i];

                if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
                {
                    if (sobel[z+p.z][y+p.y][x+p.x]==255)
                    {
                        if (SobelNeighbours(x+p.x,y+p.y,z+p.z) > 1)
						{
							found = 1;
                            break;
						}
                    }
                }
            }

            if (!found)
			{
                sobel[z][y][x]=0;
			}
        }       
    }

    /* get rid of any dupliate surface points */
/*
    for(int z = 0; z<SIZE; z++)
    for(int x = 0; x<SIZE; x++)
	{
		int prev = 0;
		for(int y = 0; y<SIZE; y++)
		{
			if (sobel[z][y][x]==255)
			{
				if (prev>2)
					sobel[z][y][x]=0;
				prev++;
			}
			else
			{
				prev = 0;
			}
		}
	}
*/
}

// Temporarily use the sobel array for this
// sobel will be overwritten with the output of sobel filtering later
int Pascal[11][11] = { {1,0,0,0,0,0,0,0,0,0,0},
				   {1,1,0,0,0,0,0,0,0,0,0},
				   {1,2,1,0,0,0,0,0,0,0,0},
				   {1,3,3,1,0,0,0,0,0,0,0},
				   {1,4,6,4,1,0,0,0,0,0,0},
				   {1,5,10,10,5,1,0,0,0,0,0},
				   {1,6,15,20,15,6,1,0,0,0,0},
				   {1,7,21,35,35,21,7,1,0,0,0},
				   {1,8,28,56,70,56,28,8,1,0,0},
				   {1,9,36,84,126,126,84,36,9,1,0},
				   {1,10,45,120,210,252,210,120,45,10,1},
                 };
				   
void GuassianSmooth(int w)
{
	int xo,yo,zo;
	int p=1<<(w-1);
	
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
		int t = 0;
		
		for(int i = 0; i<w; i++)
		{
			xo = x+i-w/2;
			if (xo<0) xo = -xo;
			if (xo>SIZE-1) xo = 2*SIZE-xo-1;
			
			t += Pascal[w-1][i]*volume[z][y][xo];
		}
		
		sobel[z][y][x] = t/p;
	}

    for(int z = 0; z<SIZE; z++)
    for(int x = 0; x<SIZE; x++)
    for(int y = 0; y<SIZE; y++)
    {
		int t = 0;
		
		for(int i = 0; i<w; i++)
		{
			yo = y+i-w/2;
			if (yo<0) yo = -yo;
			if (yo>SIZE-1) yo = 2*SIZE-yo-1;
			
			t += Pascal[w-1][i]*sobel[z][yo][x];
		}
		
		sobel[z][y][x] = t/p;
	}

    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    for(int z = 0; z<SIZE; z++)
    {
		int t = 0;
		
		for(int i = 0; i<w; i++)
		{
			zo = z+i-w/2;
			if (zo<0) zo = -zo;
			if (zo>SIZE-1) zo = 2*SIZE-zo-1;
			
			t += Pascal[w-1][i]*sobel[zo][y][x];
		}
		
		sobel[z][y][x] = t/p;
	}
}

void Laplace(void)
{
	GuassianSmooth(11);
	
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
        int xm1 = x?x-1:x+1, ym1 = y?y-1:y+1, zm1 = z?z-1:z+1;
        int xp1 = (x<SIZE-1)?x+1:x-1, yp1 = (y<SIZE-1)?y+1:y-1, zp1 = (z<SIZE-1)?z+1:z-1;
                
        int l;
    
		// Use sobel rather than volume because we have temporarily used sobel for doing the guassian smoothing
        l = 2 * (int)sobel[zm1][ym1][xm1] + 2 * (int)sobel[zm1][ym1][xp1];
        l += 3 * (int)sobel[zm1][y][xm1]  + 3 * (int)sobel[zm1][y][xp1];
        l += 2 * (int)sobel[zm1][yp1][xm1] + 2 * (int)sobel[zm1][yp1][xp1];
        l += 3 * (int)sobel[z][ym1][xm1]  + 3 * (int)sobel[z][ym1][xp1];
        l += 6 * (int)sobel[z][y][xm1] + 6 * (int)sobel[z][y][xp1];
        l += 3 * (int)sobel[z][yp1][xm1] + 3 * (int)sobel[z][yp1][xp1];
        l += 2 * (int)sobel[zp1][ym1][xm1] + 2 * (int)sobel[zp1][ym1][xp1];
        l += 3 * (int)sobel[zp1][y][xm1] + 3 * (int)sobel[zp1][y][xp1];
        l += 2 * (int)sobel[zp1][yp1][xm1] + 2 * (int)sobel[zp1][yp1][xp1];

        l += 3 * (int)sobel[zm1][ym1][x];
        l += 6 * (int)sobel[zm1][y][x];
        l += 3 * (int)sobel[zm1][yp1][x];
        l += 6 * (int)sobel[z][ym1][x];
        l += -88 * (int)sobel[z][y][x];
        l += 6 * (int)sobel[z][yp1][x];
        l += 3 * (int)sobel[zp1][ym1][x];
        l += 6 * (int)sobel[zp1][y][x];
        l += 3 * (int)sobel[zp1][yp1][x];
		
		l = 128+l/10;
        
        if (l<0)
			laplace[z][y][x] = 0;
		else if (l>255)
			laplace[z][y][x] = 255;
		else
			laplace[z][y][x] = l;

    }
}

void sobel_and_processed(void)
{
    for(int x = 0; x<SIZE; x++)
    for(int y = 0; y<SIZE; y++)
    for(int z = 0; z<SIZE; z++)
        if (processed[z][y][x]==PR_EMPTY)
        {
            sobel[z][y][x]=0;
//            sobel_orig[z][y][x]=0;
        }
}

void AdaptiveHistogramEq(void)
{
	static int cum_freq[256],freq[256];
	
	int window = 6;
	int xom,yom,zom;
	
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
	{
	  for(int i = 0; i<256; i++)
		freq[i]=0;
	  
	  xom = 0; yom = y-window; zom = z-window;
	  if (yom<0) yom=0;	  
	  if (zom<0) zom=0;
	  if (yom>SIZE-2*window-1) yom = SIZE-2*window-1;	  
	  if (zom>SIZE-2*window-1) zom = SIZE-2*window-1;

	  for(int zo = zom; zo < zom+2*window+1; zo++)	  
	  for(int yo = yom; yo < yom+2*window+1; yo++)	  
	  for(int xo = xom; xo < xom+2*window+1; xo++)
	  {
		freq[volume[zo][yo][xo]]++;
	  }	

  	  for(int i = 0, tot=0; i<256; i++)
	  {
		tot += freq[i];
		cum_freq[i]=tot;
	  }
	  
      for(int x = 0; x<SIZE; x++)
	  {
		  if (processed[z][y][x]==PR_SCROLL)
		  {
             volume_ahe[z][y][x] = (256*cum_freq[volume[z][y][x]])/cum_freq[255];
          }
		  
		  // Update the cumulative frequency table if necessary...
		  if (x>=window && x<SIZE-window-1)
		  {
	        for(int zo = zom; zo < zom+2*window+1; zo++)	  
	        for(int yo = yom; yo < yom+2*window+1; yo++)	  
	        {
		      freq[volume[zo][yo][x-window]]--;
		      freq[volume[zo][yo][x+window+1]]++;
	        }

  	        for(int i = 0, tot=0; i<256; i++)
	        {
		      tot += freq[i];
		      cum_freq[i]=tot;
	        }
		  }		  
	  }
	}
}

void KeepFillable(void)
{
/*	
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
		processed[z][y][x] = (processed[z][y][x]==PR_SCROLL && volume_ahe[z][y][x] > 110) ? PR_SCROLL : PR_EMPTY;
	}
*/			 
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
        if (processed[z][y][x] == PR_SCROLL && !fillableVoxelBasic(x,y,z))
            processed[z][y][x] = PR_TMP;
    }

    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {
        if (processed[z][y][x] == PR_TMP)
            processed[z][y][x] = PR_EMPTY;
    }
	
}

int findAndFill(int fillValue)
{
    int r = findEmptyAndStartFill(fillValue);
        
     return r;
}

int findAndFillAll(int max)
{
    int volinc,totalvol;
    
	int i;
	
    for(i = 0; findAndFill(i+PR_FILL_START) && i<max; i++)
    {
        totalvol = floodFillContinue(i+PR_FILL_START);
        
         if (i%100==0) printf("findAndFill %d, volume = %d\n",i,totalvol);
		
        regionVolumes[i] = totalvol;
    }
	
	return i;
}

/* if not building as a DLL, then the pipeline is run from main */

int main(int argc, char *argv[])
{
	if (argc != 3 && argc != 6)
	{
		printf("Usage: scrollprocess <image-directory> <output-directory> [x3 y3 z3]\n");
		return -1;
	}

	if (argc==6)
	{
	    for(int j = 0; j<3; j++)
	        normalVector[j] = atoi(argv[j+3]);
	}
	
	printf("Normal vector normalised to length 1000:\n");

	double magnitude = sqrt((double)(normalVector[0]*normalVector[0]+normalVector[1]*normalVector[1]+normalVector[2]*normalVector[2]));
		
	for(int j = 0; j<3; j++)
	{
		normalVector[j] /= (magnitude/1000);
			
		printf("%d ", normalVector[j]);
	}
			
	// assume that the last 5 digits of the image directory are the z-offset
	int zOffset = 0;
	
	if (strlen(argv[1])>=5)
	  zOffset = atoi(argv[1]+strlen(argv[1])-5);
	
	printf("zOffset=%d\n",zOffset);
	
    setFoldersAndZOffset(argv[1],argv[2],zOffset);  
    loadTiffs();
	
    dilate(110);
    dilate(110);
    dilate(110);
//    dilate(90);
//    dilate(90);
	
//	AdaptiveHistogramEq();
	Sobel();
	KeepFillable();
	
//	Laplace();
//	Sobel();
        
    int i = findAndFillAll(300000);
    printf("Found %d volumes\n",i);

    printf("Unfilling...\n");               
        
	for(int j = 0; j<i; j++)
        if (getRegionVolume(j) <= PATCH_EXPORT_LIMIT)
		{
            printf("Unfilling %d\n",j);
            unFill(j+PR_FILL_START);
		}
		
    for(int j = 0; j<5; j++)
		dilateFilledAreas();

    printf("Rendering...\n");               
    for(int j = 0; j<i; j++)
	{
        printf("%d volume %d\n",j,getRegionVolume(j));
        if (getRegionVolume(j) > PATCH_EXPORT_LIMIT)
	    {
            printf("Rendering %d\n",j);
            render(1,6,j+PR_FILL_START);
        }
	}
}

