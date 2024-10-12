/*
 Will Stevens, October 2024

 Connected components of a tif stack 
 */


#include <stdint.h>
#include <math.h>
#include <string.h>

#include "tiffio.h"

#define PR_FILL_START 3

#define SIZE 512

#define FOLDERNAME_LENGTH 256
#define FILENAME_LENGTH 300
char imageFolder[FOLDERNAME_LENGTH]="";
char outputFolder[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

uint32_t volume[SIZE][SIZE][SIZE];

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

int fillableVoxel(int x, int y, int z, unsigned c)
{
	return volume[z][y][x]==c;
}

// Must only be called if voxel is known to be fillable
void floodFillStart(unsigned x, unsigned y, unsigned z, unsigned v)
{	    
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

int findEmptyAndStartFill(unsigned c, unsigned v)
{
    static int x = 0, y = 0, z = 0;

    while(1==1)
    {
        if (fillableVoxel(x,y,z,c))
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

int floodFillContinue(unsigned c, unsigned v)
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

        if (fillableVoxel(x,y,z,c))
        {	
        volume[z][y][x]=v;
    
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
                    uint8_t v1 = ((uint8_t *)buf)[i];
                    volume[m][row][i] = v1;
					if (v1!=0 && v1 != 1 && v1 != 2)
					{
						printf("unexpected v1 %d\n",v1);
						exit(1);
					}
                }
            }
            _TIFFfree(buf);
        }
    
        TIFFClose(tif);
    }
}

int findAndFill(unsigned c, unsigned fillValue)
{
    int r = findEmptyAndStartFill(c,fillValue);
        
    return r;
}

int findAndFillAll(unsigned c, unsigned max)
{
    int totalvol;
    
	int i;
	
    for(i = 0; findAndFill(c,i+PR_FILL_START) && i<max; i++)
    {
        totalvol = floodFillContinue(c,i+PR_FILL_START);
        
         if (i%100==0) printf("findAndFill %d, volume = %d\n",i,totalvol);
		
        regionVolumes[i] = totalvol;
    }
	
	return i;
}

void render(unsigned fill)
{
    char fname[FILENAME_LENGTH];
    sprintf(fname,"%s/v_%d.csv",outputFolder,fill);
    FILE *f = fopen(fname,"w");
    
    
    for(int x=fillExtent[fill][0]; x<=fillExtent[fill][3]; x++)
    for(int z=fillExtent[fill][2]; z<=fillExtent[fill][5]; z++)
    {
        int foundYet = 0;
        for (int y=fillExtent[fill][1]; y<=fillExtent[fill][4]; y++)
        {
            int v = volume[z][y][x];
            if (v==fill && !foundYet)
            {             
              fprintf(f,"%d,%d,%d\n",x,y,z);

              foundYet = 1;			  
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

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: connected <image-directory> <output-directory>\n");
		return -1;
	}

    setFoldersAndZOffset(argv[1],argv[2],0);  
    loadTiffs();
	
        
    int i = findAndFillAll(2,300000);
    printf("Found %d volumes\n",i);
        		
    printf("Rendering...\n");               
    for(int j = 0; j<i; j++)
	{
        if (getRegionVolume(j) > 80000)
	    {
            printf("%d volume %d\n",j,getRegionVolume(j));
            printf("Rendering %d\n",j);
            render(j+PR_FILL_START);
        }
	}
}

