#include <stdint.h>
#include <math.h>

#include "tiffio.h"

#define SIZE 512

#define BLOCK_NO "005"
#define Z_OFFSET 5800 // for 005
//#define Z_OFFSET 5288 // for 105
//#define Z_OFFSET 4776 // for 205

/* This is the image */ /* 128 Mb */
unsigned char volume[SIZE][SIZE][SIZE];

/* This is a working area for processing the image */ /* 512 Mb */
unsigned processed[SIZE][SIZE][SIZE];

/* Image processed by sobel edge detection */ /* 128 Mb */
unsigned char sobel[SIZE][SIZE][SIZE];

//unsigned char sobel_orig[SIZE][SIZE][SIZE]; /* 128 Mb */

/* A slice of the processed image, RGB coloured */
unsigned processed_coloured[SIZE][SIZE];

unsigned char rendered[SIZE][SIZE];

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

unsigned fillValue = PR_FILL_START;

/* allow for 1000000 different fill colours */
int fillExtent[1000000][6]; /* xmin,ymin,zmin,xmax,ymax,zmax for each fill colour */
int regionVolumes[1000000];

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
        if (z+zo>=0 && z+zo<SIZE && y+yo>=0 && y+yo<SIZE && x+xo>=0 && x+xo<SIZE)
        {
			if (processed[z+zo][y+yo][x+xo]==PR_EMPTY)
			{
				return 1;
			}
		}
		
    return 0;
}

int fillableVoxel(int x, int y, int z, int v)
{
	static int rcounter = 0;
	//return fillableVoxelBasic(x,y,z);
	
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
    
	// d2 values when count>=5
	// 62 too permissive
	// 30 is okay but a little permissive
	// 25 is not okay - too many small patches
	
	// d2 < 29 && count>=10 seems quite good
    int r = d2 < 29 && count>=70;
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
	
    fillValue = v;
    
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

#define FILL_CASE(xo,yo,zo,test) \
    if (test>=0 && test<SIZE) \
    { \
        fillQueue[fillQueueHead++]=x+xo; \
        fillQueue[fillQueueHead++]=y+yo; \
        fillQueue[fillQueueHead++]=z+zo; \
      \
        if (fillQueueHead>=FILLQUEUELENGTH) \
            fillQueueHead-=FILLQUEUELENGTH; \
    } 

int floodFillContinue(void)
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
      
		FILL_CASE(-1,0,0,x-1)
		FILL_CASE(1,0,0,x+1)
		FILL_CASE(0,-1,0,y-1)
		FILL_CASE(0,1,0,y+1)
		FILL_CASE(0,0,-1,z-1)
		FILL_CASE(0,0,1,z+1)			
        }

	}
    
    return fillCount;
}

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
        char fname[100];
        sprintf(fname,"../construct/e" BLOCK_NO "/e" BLOCK_NO "_0%d.tif",Z_OFFSET+m);
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
    char fname[100];
    sprintf(fname,"../construct/s" BLOCK_NO "/v" BLOCK_NO "_%d.csv",fill);
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
    
    sprintf(fname,"../construct/s" BLOCK_NO "/x" BLOCK_NO "_%d.csv",fill);
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
        sobel[z][y][x] = (dy<0?sqrt(dx*dx+dy*dy+dz*dz):0)>220?255:0;
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

int findAndFill(void)
{
    int r = findEmptyAndStartFill(fillValue);
        
     return r;
}

int findAndFillAll(int max)
{
    int volinc,totalvol;
    
	int i;
	
    for(i = 0; findAndFill() && i<max; i++)
    {
        if (i%100==0) printf("findAndFill %d\n",i);

        totalvol = floodFillContinue();

        fillValue+=1;
        if (fillValue>1000000)
            fillValue=PR_FILL_START; 
        
        regionVolumes[i] = totalvol;
    }
	
	return i;
}