#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SIZE 512

uint16_t volume[SIZE][SIZE][SIZE];

uint16_t points[SIZE*SIZE*SIZE/10][3];
uint32_t numPoints = 0;

uint16_t pointsVisited[SIZE*SIZE*SIZE/10][3];
uint32_t numPointsVisited = 0;

uint16_t projection[SIZE][SIZE];

#define FILLQUEUELENGTH (SIZE*SIZE*6*4)
uint16_t fillQueue[FILLQUEUELENGTH];
uint32_t fillQueueHead = 0;
uint32_t fillQueueTail = 0;

#define SURFACE 1
#define PLUG 2
#define SEEK 3
#define FILL_START 4

void loadVolume(char *v)
{
	int x,y,z;
	
	FILE *f = fopen(v,"r");
	
	if (f)
	{
		while (fscanf(f,"%d,%d,%d\n",&x,&y,&z) != EOF)
		{
			points[numPoints][0]=x;
			points[numPoints][1]=y;
			points[numPoints][2]=z;
			numPoints++;
			
			volume[z][y][x]=SURFACE;
		}
	}
}

void resetFill(void)
{
	int x,y,z;
	
	for(int i = 0; i<numPointsVisited; i++)
	{
		x = pointsVisited[i][0];
		y = pointsVisited[i][1];
		z = pointsVisited[i][2];
		
		if (volume[z][y][x] != PLUG)
			volume[z][y][x] = SURFACE;
		
		projection[z][x]=0;
	}
	
	numPointsVisited = 0;
}

int testProjection(void)
{
	for(int x = 0; x<SIZE; x++)
		for(int y = 0; y<SIZE; y++)
			if (projection[y][x] != 0)
				return 0;
	return 1;
}

#define FILL_CASE(xo,yo,zo,v,test) \
    if (test>=0 && test<SIZE) \
    { \
        fillQueue[fillQueueHead++]=*x+xo; \
        fillQueue[fillQueueHead++]=*y+yo; \
        fillQueue[fillQueueHead++]=*z+zo; \
        fillQueue[fillQueueHead++]=v; \
      \
        if (fillQueueHead>=FILLQUEUELENGTH) \
            fillQueueHead-=FILLQUEUELENGTH; \
    } 

int floodFillSeek(int *x, int *y, int *z, int seekValue)
{   
	int fillValue;
	
    fillQueueHead = 0;
    fillQueueTail = 0;
    fillQueue[fillQueueHead++]=*x;
    fillQueue[fillQueueHead++]=*y;
    fillQueue[fillQueueHead++]=*z;
    fillQueue[fillQueueHead++]=SEEK; // Don't really need this because it is constant, but need to have 4 values in the queue
    	
    if (fillQueueHead>=FILLQUEUELENGTH)
        fillQueueHead-=FILLQUEUELENGTH;
    
    while(fillQueueHead != fillQueueTail)
    {
        *x = fillQueue[fillQueueTail++];
        *y = fillQueue[fillQueueTail++];
        *z = fillQueue[fillQueueTail++];
        fillValue = fillQueue[fillQueueTail++];
        
        if (fillQueueTail>=FILLQUEUELENGTH)
            fillQueueTail-=FILLQUEUELENGTH;

        if (volume[*z][*y][*x]==SURFACE || volume[*z][*y][*x]>=FILL_START)
        {
			if (volume[*z][*y][*x] == seekValue)
			{
				return seekValue;
			}
			
			volume[*z][*y][*x]=fillValue;
			
			// Duplicate points will be added to pointsVisit, but this doesn't matter
            pointsVisited[numPointsVisited][0]=*x;
            pointsVisited[numPointsVisited][1]=*y;
            pointsVisited[numPointsVisited++][2]=*z;
			
			FILL_CASE(-1,0,0,fillValue,*x-1)
			FILL_CASE(1,0,0,fillValue,*x+1)
			FILL_CASE(0,-1,0,fillValue,*y-1)
			FILL_CASE(0,1,0,fillValue,*y+1)
			FILL_CASE(0,0,-1,fillValue,*z-1)
			FILL_CASE(0,0,1,fillValue,*z+1)			
        }

	}
    
    return 0;
}

int floodFill(int *x, int *y, int *z)
{   
    int fillValue;

    fillQueueHead = 0;
    fillQueueTail = 0;
    fillQueue[fillQueueHead++]=*x;
    fillQueue[fillQueueHead++]=*y;
    fillQueue[fillQueueHead++]=*z;
    fillQueue[fillQueueHead++]=FILL_START;
    	
    if (fillQueueHead>=FILLQUEUELENGTH)
        fillQueueHead-=FILLQUEUELENGTH;
    
    while(fillQueueHead != fillQueueTail)
    {
        *x = fillQueue[fillQueueTail++];
        *y = fillQueue[fillQueueTail++];
        *z = fillQueue[fillQueueTail++];
        fillValue = fillQueue[fillQueueTail++];
        
        if (fillQueueTail>=FILLQUEUELENGTH)
            fillQueueTail-=FILLQUEUELENGTH;

        if (volume[*z][*y][*x]==SURFACE)
        {
			if (projection[*z][*x]==0 || (projection[*z][*x]>=*y+1-5 && projection[*z][*x]<=*y+1+5))
			{
				projection[*z][*x]=*y+1;
			}
			else
			{
				return fillValue;
			}
			
			volume[*z][*y][*x]=fillValue;
            pointsVisited[numPointsVisited][0]=*x;
            pointsVisited[numPointsVisited][1]=*y;
            pointsVisited[numPointsVisited++][2]=*z;
			
			FILL_CASE(-1,0,0,fillValue+1,*x-1)
			FILL_CASE(1,0,0,fillValue+1,*x+1)
			FILL_CASE(0,-1,0,fillValue+1,*y-1)
			FILL_CASE(0,1,0,fillValue+1,*y+1)
			FILL_CASE(0,0,-1,fillValue+1,*z-1)
			FILL_CASE(0,0,1,fillValue+1,*z+1)			
        }

	}
    
    return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: holefinder <filename>\n");
		return -1;
	}
	
	loadVolume(argv[1]);

	
	printf("Total points %d\n",numPoints);
	
	int x,y,z;
	
	int doing = 1000;
	
	srand(1);
	
	int iters = 0;
	while(doing && iters++<400000)
	{	
//		if (!testProjection())
//		{
//			printf("Projection not empty\n");
//		}

        int pt = rand()%numPoints;
		
		x = points[pt][0];
		y = points[pt][1];
		z = points[pt][2];

//		printf("Starting from [%d,%d,%d],\n",x,y,z);
		
	    // x,y,z will be set to the first point that causes overlap
		if (floodFill(&x,&y,&z) != 0)
		{
			doing = 1000;
			
			// x,y,z will be set to the first point that causes overlap
//		    printf("First overlap point [%d,%d,%d],\n",x,y,z);
			
			resetFill();

			// Fill again from that point until overlap
			int fv = floodFill(&x,&y,&z)-FILL_START;

//		    printf("Nearest overlap found [%d,%d,%d],\n",x,y,z);
			
		    // Find the fillValue that plug will have
			int plugFillValue = fv/2 + FILL_START;
		
			// Now without resetting fill, flood fill from x,y,z until we reach the first plugFillValue
			if (floodFillSeek(&x,&y,&z,plugFillValue) == plugFillValue)
			{
				printf("[%d,%d,%d],\n",x,y,z);
				volume[z][y][x] = PLUG;
			}

			resetFill();
		}
		else
		{
			resetFill();
			doing--;
		}
	}
		
	return 0;
}