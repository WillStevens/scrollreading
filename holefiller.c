/*
 Will Stevens, August 2024
 
 An implementation of the DAFF algorithm described in report.pdf
 This was written to experiment with creating non-intersecting surfaces in cubic volumes of the Herculanium Papyri.
 
 Invoke by giving the name of a CSV file where each line is the x,y,z coordinates of a point (where x,y,z are all >=0 and <SIZE).
 
 It will print a list of plug voxels that can be pasted into a python program.
 
 Released under GNU Public License V3
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

#define SIZE 512

typedef struct
{
	uint16_t value;
    // 6 LSB store the fillability of each neighbouring voxel.
    // This makes filling more efficient because there is no need to visit unfillable neighbours
	uint8_t neighbourFillable;
} cell;

cell volume[SIZE][SIZE][SIZE];

uint16_t points[SIZE*SIZE*SIZE/5][3];
uint32_t numPoints = 0;

uint16_t pointsVisited[SIZE*SIZE*SIZE/5][3];

uint32_t numPointsVisited = 0;

// Two vectors that define the projection plane
// The third vector in this array is the normal to the projection plane, calculated from the other two
int32_t planeVectors[3][3] = 
	{ { 1, 0, 0 },
	  { 0, 0, 1 } };

#define PROJECTION_SIZE 2048
#define PROJECTION_BLANK INT32_MIN
int32_t projection[PROJECTION_SIZE][PROJECTION_SIZE];

// FILLQUEUELENGTH = 2^23 so that it can be wrapped around using an AND operation
#define FILLQUEUELENGTH     0x00800000
#define FILLQUEUELENGTHMASK 0x007fffff
uint16_t fillQueue[FILLQUEUELENGTH];
uint32_t fillQueueHead = 0;
uint32_t fillQueueTail = 0;

#define EMPTY 0
#define SEEK 1
#define PLUG 2
#define PLUG_TMP 3 // Used when filling to prevent revisiting a plug
#define SURFACE 4
#define FILL_START 5

#define XYZTOINDEX_HASHSIZE 17292347
struct {
  uint16_t x,y,z;
  uint32_t index;
} XYZToIndexHashTable[XYZTOINDEX_HASHSIZE];

// TODO - this hash function could be better
int XYZToIndexHashFunction(int x, int y, int z)
{
	return (x*62137+y*2137+z*17)% XYZTOINDEX_HASHSIZE;
}

int LookupPointIndex(int x, int y, int z)
{
	int hash = XYZToIndexHashFunction(x,y,z);
	int oHash = hash;
	
	while(XYZToIndexHashTable[hash].x != x+1 ||
          XYZToIndexHashTable[hash].y != y+1 ||
		  XYZToIndexHashTable[hash].z != z+1)
    {
	   hash = (hash+1)%XYZTOINDEX_HASHSIZE;
	   
	   if (hash == oHash)
	   {
		 printf("Hash error in LookupPointIndex");
	     exit(-1);
	     return -1;
	   }
    }
	
	return XYZToIndexHashTable[hash].index;
}

/* Get the point index given X,Y,Z and remove the point from the hash table */
int pointIndexFromXYZRemove(int x, int y, int z)
{
	int hash = XYZToIndexHashFunction(x,y,z);
	int oHash = hash;
	
//	printf("%d : %d,%d,%d\n",hash,XYZToIndexHashTable[hash].x,XYZToIndexHashTable[hash].y,XYZToIndexHashTable[hash].z);
	while(XYZToIndexHashTable[hash].x != x+1 ||
          XYZToIndexHashTable[hash].y != y+1 ||
		  XYZToIndexHashTable[hash].z != z+1)
    {
	   hash = (hash+1)%XYZTOINDEX_HASHSIZE;
//	   printf("%d : %d,%d,%d\n",hash,XYZToIndexHashTable[hash].x,XYZToIndexHashTable[hash].y,XYZToIndexHashTable[hash].z);
	   
	   if (hash == oHash)
	   {
		 printf("Hash error in pointIndexFromXYZRemove");		   
	     exit(-1);
	     return -1;
	   }
    }

	int r = XYZToIndexHashTable[hash].index;

	XYZToIndexHashTable[hash].x = 0;

    return r;	
}

/* x,y,z must not already be in the hash table */
int setPointIndexLookup(int x, int y, int z, int index)
{
	int hash = XYZToIndexHashFunction(x,y,z);
	int oHash = hash;
	
	while(XYZToIndexHashTable[hash].x != 0)
    {
	   hash = (hash+1)%XYZTOINDEX_HASHSIZE;
	   
	   if (hash == oHash)
	   { 
		 printf("Hash error in setPointIndexLookup");		      
         exit(-1);
	     return -1;
	   }
    }

	XYZToIndexHashTable[hash].x = x+1;
	XYZToIndexHashTable[hash].y = y+1;
	XYZToIndexHashTable[hash].z = z+1;
	XYZToIndexHashTable[hash].index = index;

    return 0;	
}

/* x,y,z should already be in the hash table, update the index */
int updatePointIndexLookup(int x, int y, int z, int index)
{
	int hash = XYZToIndexHashFunction(x,y,z);
	int oHash = hash;
	
	while(XYZToIndexHashTable[hash].x != x+1 ||
          XYZToIndexHashTable[hash].y != y+1 ||
		  XYZToIndexHashTable[hash].z != z+1)
    {
	   hash = (hash+1)%XYZTOINDEX_HASHSIZE;
	   
	   if (hash == oHash)
	     return -1;
    }

	XYZToIndexHashTable[hash].x = x+1;
	XYZToIndexHashTable[hash].y = y+1;
	XYZToIndexHashTable[hash].z = z+1;
	XYZToIndexHashTable[hash].index = index;

    return 0;	
}

int32_t projectN(int x, int y, int z, int n)
{
	int32_t r = (x*planeVectors[n][0]+y*planeVectors[n][1]+z*planeVectors[n][2])/1000;
	
	return r;
}

// These definitions use run-time variables
//#define projectN0(x,y,z) (((x)*planeVectors[0][0]+(y)*planeVectors[0][1]+(z)*planeVectors[0][2])/1000)
//#define projectN1(x,y,z) (((x)*planeVectors[1][0]+(y)*planeVectors[1][1]+(z)*planeVectors[1][2])/1000)
//#define projectN2(x,y,z) (((x)*planeVectors[2][0]+(y)*planeVectors[2][1]+(z)*planeVectors[2][2])/1000)

#define PLANEVECTORS_0_0 1000
#define PLANEVECTORS_0_1 0
#define PLANEVECTORS_0_2 0

#define PLANEVECTORS_1_0 0
#define PLANEVECTORS_1_1 0
#define PLANEVECTORS_1_2 1000

#define PLANEVECTORS_2_0 0
#define PLANEVECTORS_2_1 -1000
#define PLANEVECTORS_2_2 0

// These definitions used compile-time constants
#define projectN0(x,y,z) (((x)*PLANEVECTORS_0_0+(y)*PLANEVECTORS_0_1+(z)*PLANEVECTORS_0_2)/1000)
#define projectN1(x,y,z) (((x)*PLANEVECTORS_1_0+(y)*PLANEVECTORS_1_1+(z)*PLANEVECTORS_1_2)/1000)
#define projectN2(x,y,z) (((x)*PLANEVECTORS_2_0+(y)*PLANEVECTORS_2_1+(z)*PLANEVECTORS_2_2)/1000)

/* Load 'points' and populate 'volume' from a CSV file where each line contains the x,y,z coordinates of a point */
/* x,y,z must all be >=0 and < SIZE */
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

            setPointIndexLookup(x,y,z,numPoints);
			
			numPoints++;
			
			volume[z][y][x].value=SURFACE;
		}
	}
}

void populateNeighbourFillable(void)
{
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
    {		
        for(unsigned i = 0; i<6; i++)
        {
            vector p = dirVectorLookup[i];

			volume[z][y][x].neighbourFillable=volume[z][y][x].neighbourFillable<<1; 
			
            if (z+p.z>=0 && z+p.z<SIZE && y+p.y>=0 && y+p.y<SIZE && x+p.x>=0 && x+p.x<SIZE)
            {
				volume[z][y][x].neighbourFillable |= (volume[z+p.z][y+p.y][x+p.x].value==SURFACE?1:0);
            }
        }
    }
}

/* Using the list of points visit so far, reset volume and projection back to how they were after loadVolume was called, except that plugs found so far are left alone */
void resetFill(int output)
{
	int x,y,z;

    if (output)
      printf("resetFill\n");

//	printf("Test before resetFill:%d\n",(int)volume[17][503][70]);
  
	for(int i = 0; i<numPointsVisited; i++)
	{
		x = pointsVisited[i][0];
		y = pointsVisited[i][1];
		z = pointsVisited[i][2];
		
		if (volume[z][y][x].value != EMPTY)
		{
			if (volume[z][y][x].value != PLUG_TMP)
			  volume[z][y][x].value = SURFACE;
		    else
			  volume[z][y][x].value = PLUG;
		  
			if (output)
				printf("%d,%d,%d\n",x,y,z);
		}
		
		projection[projectN0(x,y,z)+PROJECTION_SIZE/2][projectN1(x,y,z)+PROJECTION_SIZE/2]=PROJECTION_BLANK;
	}
	
	numPointsVisited = 0;
	
//	printf("Test after resetFill:%d\n",(int)volume[17][503][70]);
}

/* Check that projection is empty - useful for debugging */
int testProjection(void)
{
	for(int x = 0; x<PROJECTION_SIZE; x++)
		for(int y = 0; y<PROJECTION_SIZE; y++)
			if (projection[y][x] != PROJECTION_BLANK)
				return 0;
	return 1;
}

#define FILL_CASE_LO(xo,yo,zo,v,test,neighbourMask) \
    if (test>=0 && (0 || (c->neighbourFillable&neighbourMask))) \
    { \
        fillQueue[fillQueueHead++]=*x+xo; \
        fillQueue[fillQueueHead++]=*y+yo; \
        fillQueue[fillQueueHead++]=*z+zo; \
        fillQueue[fillQueueHead++]=v; \
      \
        fillQueueHead &= FILLQUEUELENGTHMASK; \
    } 

#define FILL_CASE_HI(xo,yo,zo,v,test,neighbourMask) \
    if (test<SIZE && (0 || (c->neighbourFillable&neighbourMask))) \
    { \
        fillQueue[fillQueueHead++]=*x+xo; \
        fillQueue[fillQueueHead++]=*y+yo; \
        fillQueue[fillQueueHead++]=*z+zo; \
        fillQueue[fillQueueHead++]=v; \
      \
        fillQueueHead &= FILLQUEUELENGTHMASK; \
    } 



#define FILL_CASE(xo,yo,zo,v,neighbourMask) \
    if (c->neighbourFillable&neighbourMask) \
    { \
        fillQueue[fillQueueHead++]=*x+xo; \
        fillQueue[fillQueueHead++]=*y+yo; \
        fillQueue[fillQueueHead++]=*z+zo; \
        fillQueue[fillQueueHead++]=v; \
      \
        fillQueueHead &= FILLQUEUELENGTHMASK; \
    } 

#define FILL_CASE_NOV(xo,yo,zo,neighbourMask) \
    if (c->neighbourFillable&neighbourMask) \
    { \
        fillQueue[fillQueueHead++]=*x+xo; \
        fillQueue[fillQueueHead++]=*y+yo; \
        fillQueue[fillQueueHead++]=*z+zo; \
        fillQueueHead++; \
      \
        fillQueueHead &= FILLQUEUELENGTHMASK; \
    } 

/* Flood fill in a surface from a point looking for a particular seekValue from a previous flood fill, and stop when it is found with *x,*y,*z containing the coordinates where it is found */
int floodFillSeek(int *x, int *y, int *z, int seekValue, int tmp)
{   	
    fillQueueHead = 0;
    fillQueueTail = 0;
    fillQueue[fillQueueHead++]=*x;
    fillQueue[fillQueueHead++]=*y;
    fillQueue[fillQueueHead++]=*z;
    fillQueue[fillQueueHead++]=SEEK; // Don't really need this because it is constant, but need to have 4 values in the queue
    	    
    while(fillQueueHead != fillQueueTail)
    {
        *x = fillQueue[fillQueueTail++];
        *y = fillQueue[fillQueueTail++];
        *z = fillQueue[fillQueueTail++];
        fillQueueTail++;
        
        fillQueueTail &= FILLQUEUELENGTHMASK;

		cell *c = &(volume[*z][*y][*x]);
		
        if (c->value>SURFACE)
        {
			if (c->value == seekValue)
			{
				return seekValue;
			}
			
			c->value=SEEK;
						
			FILL_CASE_NOV(-1,0,0,0x04)
			FILL_CASE_NOV(1,0,0,0x10)
			FILL_CASE_NOV(0,-1,0,0x08)
			FILL_CASE_NOV(0,1,0,0x20)
			FILL_CASE_NOV(0,0,-1,0x01)
			FILL_CASE_NOV(0,0,1,0x02)			
        }

	}
    
    return 0;
}

/* Flood fill in a surface from a point looking for a particular seekValue from a previous flood fill, and stop when it is found with *x,*y,*z containing the coordinates where it is found */
int floodFillSeekX(int *x, int *y, int *z, int seekValue, int seekStartFillValue)
{   
    int first = 1;
	
//	printf("Call %d\n",seekValue);
	
    fillQueueHead = 0;
    fillQueueTail = 0;
    fillQueue[fillQueueHead++]=*x;
    fillQueue[fillQueueHead++]=*y;
    fillQueue[fillQueueHead++]=*z;
    fillQueue[fillQueueHead++]=SEEK; // Don't really need this because it is constant, but need to have 4 values in the queue
    	    
    while(fillQueueHead != fillQueueTail)
    {
        *x = fillQueue[fillQueueTail++];
        *y = fillQueue[fillQueueTail++];
        *z = fillQueue[fillQueueTail++];
        fillQueueTail++;
        
        fillQueueTail &= FILLQUEUELENGTHMASK;

		cell *c = &(volume[*z][*y][*x]);

//		printf("1 : %d,%d,%d : %d\n",*x,*y,*z,c->value);
		
        if (c->value>SURFACE || first)
        {
//		    printf("2 : %d,%d,%d : %d\n",*x,*y,*z,c->value);
			if (c->value == seekValue)
			{
//				printf("Return seekvalue: %d\n",seekValue);
				return seekValue;
			}
			
			c->value=SEEK;

			if (first)
			{
				pointsVisited[numPointsVisited][0]=*x;
				pointsVisited[numPointsVisited][1]=*y;
				pointsVisited[numPointsVisited++][2]=*z;
			}
			
			FILL_CASE_NOV(-1,0,0,0x04)
			FILL_CASE_NOV(1,0,0,0x10)
			FILL_CASE_NOV(0,-1,0,0x08)
			FILL_CASE_NOV(0,1,0,0x20)
			FILL_CASE_NOV(0,0,-1,0x01)
			FILL_CASE_NOV(0,0,1,0x02)

			first = 0;
        }

	}
    
    return 0;
}

/* Flood fill and project points onto a 2D plane. Stop when there is an intersection (i.e. there is already a projected point. *x,*y,*z contain the coordinates of the intersection */
int floodFill(int *x, int *y, int *z)
{   
    int fillValue;

	if (numPointsVisited != 0)
	{
		printf("Error - numPointsVisited non-zero in floodFill\n");
		exit(-1);
	}
    fillQueueHead = 0;
    fillQueueTail = 0;
    fillQueue[fillQueueHead++]=*x;
    fillQueue[fillQueueHead++]=*y;
    fillQueue[fillQueueHead++]=*z;
    fillQueue[fillQueueHead++]=FILL_START;
    	    
    while(fillQueueHead != fillQueueTail)
    {
        *x = fillQueue[fillQueueTail++];
        *y = fillQueue[fillQueueTail++];
        *z = fillQueue[fillQueueTail++];
        fillValue = fillQueue[fillQueueTail++];
        
        fillQueueTail &= FILLQUEUELENGTHMASK;

		cell *c = &(volume[*z][*y][*x]);
		
        if (c->value==SURFACE)
        {
			c->value=fillValue;
            pointsVisited[numPointsVisited][0]=*x;
            pointsVisited[numPointsVisited][1]=*y;
            pointsVisited[numPointsVisited++][2]=*z;		

	        // Distance of *X,*y,*z from the projection plane - positive or negative
			int depth = projectN2(*x,*y,*z);
			
			if (projection[projectN0(*x,*y,*z)+PROJECTION_SIZE/2][projectN1(*x,*y,*z)+PROJECTION_SIZE/2]==PROJECTION_BLANK || 
			     (projection[projectN0(*x,*y,*z)+PROJECTION_SIZE/2][projectN1(*x,*y,*z)+PROJECTION_SIZE/2]>=depth-5 && projection[projectN0(*x,*y,*z)+PROJECTION_SIZE/2][projectN1(*x,*y,*z)+PROJECTION_SIZE/2]<=depth+5))
			{
				projection[projectN0(*x,*y,*z)+PROJECTION_SIZE/2][projectN1(*x,*y,*z)+PROJECTION_SIZE/2]=depth;
			}
			else
			{
				if (numPointsVisited==0)
				{
					printf("Error - numPointsVisited is zero when intersection found at %d,%d,%d (%d)\n",*x,*y,*z,projection[projectN0(*x,*y,*z)+PROJECTION_SIZE/2][projectN1(*x,*y,*z)+PROJECTION_SIZE/2]);
					exit(-2);
				}

//				printf("#Intersection occurred at at %d,%d,%d : %d\n",*x,*y,*z,projection[projectN(*x,*y,*z,0)+PROJECTION_SIZE/2][projectN(*x,*y,*z,1)+PROJECTION_SIZE/2]);
				
				return fillValue;
			}
			
			
			FILL_CASE(-1,0,0,fillValue+1,0x04)
			FILL_CASE(1,0,0,fillValue+1,0x10)
			FILL_CASE(0,-1,0,fillValue+1,0x08)
			FILL_CASE(0,1,0,fillValue+1,0x20)
			FILL_CASE(0,0,-1,fillValue+1,0x01)
			FILL_CASE(0,0,1,fillValue+1,0x02)			
        }
        else if (c->value==PLUG)
        {
			/* If it's a plug then we want to add it to the visited list so that it gets exported as part of the surface, but
			 * we don't want to propagate fill through it */
            pointsVisited[numPointsVisited][0]=*x;
            pointsVisited[numPointsVisited][1]=*y;
            pointsVisited[numPointsVisited++][2]=*z;
			
			c->value=PLUG_TMP;
		}

	}
    
    return 0;
}

void removePointAtXYZ(int x, int y, int z)
{
	int index = pointIndexFromXYZRemove(x,y,z);
/*
	if (x==74 && y==75 && z==50)
	{
		printf("At %d,%d,%d index = %d\n",x,y,z,index);
		printf("Point at this index is %d,%d,%d\n",points[index][0],points[index][1],points[index][2]);
	}
*/	
	volume[z][y][x].value = EMPTY;
	
	numPoints--;
				
	points[index][0] = points[numPoints][0];
	points[index][1] = points[numPoints][1];
	points[index][2] = points[numPoints][2];
				
	/* Update the index for this point */
	updatePointIndexLookup(points[index][0],points[index][1],points[index][2],index);
}

int main(int argc, char *argv[])
{
	if ((argc != 3 && argc != 9) || strlen(argv[1])<7)
	{
		printf("Usage: holefiller <filename> <outputdir> [x1 y1 z1 x2 y2 z2]\n");
		return -1;
	}
	
	if (argc==9)
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
	
	for(int i=0; i<PROJECTION_SIZE; i++)
	  for(int j=0; j<PROJECTION_SIZE; j++)
		projection[i][j]=PROJECTION_BLANK;
	
	loadVolume(argv[1]);
	populateNeighbourFillable();
	
	int vnum = 0;

	{
		char *s;
		for(s = argv[1]+strlen(argv[1]); s!=argv[1] && *s!='_'; s--);
		
		for(s++; *s && *s != '.'; s++)
		{
			vnum = vnum*10 + ((*s)-'0');
		}					
	}
	
	printf("#Total points %d\n",numPoints);
	
	int x,y,z;
	
	srand(5);
	
	int outputIndex = 0;
	
	int iters = 0;
	while(numPoints > 0 && iters++ < 16000000)
	{	
//		if (!testProjection())
//		{
//			printf("Projection not empty 1\n");
//		}

        /* Pick a random point */
        int pt = rand()%numPoints;
		
		x = points[pt][0];
		y = points[pt][1];
		z = points[pt][2];

		// If starting from a zero point, it means that some point was not correctly removed from
		// the pointlist
//		printf("#Starting from [%d,%d,%d] : %d,\n",x,y,z,volume[z][y][x]);
		
	    // If floodFill returns non-zero then x,y,z will be set to the first point that causes intersection
		if (floodFill(&x,&y,&z) != 0)
		{			
			// x,y,z will be set to the first point that causes overlap
//		    printf("First overlap point [%d,%d,%d],\n",x,y,z);
			
			int lastFv;
			int fv;
			int seekStartFillValue;
			
			for(int seekIter = 0; seekIter<2 || lastFv > fv; seekIter++)
			{
				lastFv = fv;
				
				/* Reset volume and projection (keeping previously found plugs intact) */
				resetFill(0);

//				if (!testProjection())
//				{
//					printf("Projection not empty 2\n");
//				}

				// Fill again from that point until overlap
				seekStartFillValue = floodFill(&x,&y,&z);
				fv = seekStartFillValue-FILL_START;
			}
			
//		    printf("Nearest overlap found [%d,%d,%d],\n",x,y,z);
			
		    // Find the fillValue that plug will have - a midpoint between where we started and where we finished
			int plugFillValue = fv/2 + FILL_START;
		
			// Now without resetting fill, flood fill from x,y,z until we reach the first plugFillValue
			if (floodFillSeek(&x,&y,&z,plugFillValue,seekStartFillValue) == plugFillValue)
			{
				/* Output the coordinates of the plug */
//				printf("[%d,%d,%d], #%d\n",x,y,z,(int)volume[z][y][x]);

				/* Reset volume and projection */
				resetFill(0);
												
				/* Remove the plugged voxel from the point list */
				removePointAtXYZ(x,y,z);
				
				/* Set it to PLUG so that it will be included in exported surfaces */
				volume[z][y][x].value = PLUG;

			}
            else
			{
				/* Reset volume and projection */
				resetFill(0);
			}
		}
		else
		{
			/* Flood fill didn't result in overlap, so this must be a surface patch ready to export */
			/* Export it and remove the visited points from the points array */
			printf("#Exporting v%d_%d.csv (%d points)\n",vnum,outputIndex,numPointsVisited);
			
			int xmin=SIZE,ymin=SIZE,zmin=SIZE,xmax=-1,ymax=-1,zmax=-1;
			FILE *f = NULL;
			
			// Don't export small regions
			if (numPointsVisited >= 2000)
			{
				char fname[100];
			
				sprintf(fname,"%s/v%d_%d.csv",argv[2],vnum,outputIndex);
			
				f = fopen(fname,"w");
			}
			
			
			for(int i = 0; i<numPointsVisited; i++)
			{
				/* Export the point */
				if (f)
				  fprintf(f,"%d,%d,%d\n",pointsVisited[i][0],pointsVisited[i][1],pointsVisited[i][2]);
			  
			    if (pointsVisited[i][0]<xmin) xmin=pointsVisited[i][0];
			    if (pointsVisited[i][1]<ymin) ymin=pointsVisited[i][1];
			    if (pointsVisited[i][2]<zmin) zmin=pointsVisited[i][2];
			    if (pointsVisited[i][0]>xmax) xmax=pointsVisited[i][0];
			    if (pointsVisited[i][1]>ymax) ymax=pointsVisited[i][1];
			    if (pointsVisited[i][2]>zmax) zmax=pointsVisited[i][2];
				
				/* Remove the point from the points array */
				if (volume[pointsVisited[i][2]][pointsVisited[i][1]][pointsVisited[i][0]].value != PLUG_TMP)
				  removePointAtXYZ(pointsVisited[i][0],pointsVisited[i][1],pointsVisited[i][2]);
			}
			
			if (f)
			{
				fclose(f);

				char fname[100];

				sprintf(fname,"%s/x%d_%d.csv",argv[2],vnum,outputIndex);
			
				f = fopen(fname,"w");
				
				if (f)
				{		
					fprintf(f,"%d,%d,%d,%d,%d,%d",xmin,ymin,zmin,xmax,ymax,zmax);    
					fclose(f);
				}
			}
			
			outputIndex++;

			/* Reset volume and projection (keeping previously found plugs intact) */
			resetFill(0);
		}
	}
		
    printf("Holefiller finished, iters = %d, numPoints = %d\n",iters,numPoints);
	return 0;
}