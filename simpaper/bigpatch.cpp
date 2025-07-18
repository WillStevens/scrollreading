/*
Currently surface is represented as CSV containing lots of

gx,gy,x,y,z

Operations on surfaces are: 
- merge in new patch
- align patch with surface
   load points from both patches
   use these to align
   
The alignment algorithm could easily be modified to load only those points that are near points in the other patch

So a representation for surfaces that boxes up points would work

Like a zarr, space is split up into chunks

Each chunk file is a list:
gx,gy,x,y,z

of all the points in the chunk
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <tuple>
#include <vector>
#include <map>
#include <algorithm>

typedef std::tuple<int,int,int> chunkIndex;
typedef std::tuple<float,float,float,float,float> gridPoint;

#define PATCH_CHUNK_SIZE 64

typedef struct
{
    int locationRootLength;
    char *location;
} BigPatch;

BigPatch *OpenBigPatch(const char *location)
{
	BigPatch *z = (BigPatch *)malloc(sizeof(BigPatch));
	
	z->locationRootLength = strlen(location);
	
	z->location = (char *)malloc(z->locationRootLength + 1 + 100);
	
    strcpy(z->location,location);

    return z;	
}

void CloseBigPatch(BigPatch *z)
{
	free(z->location);
	free(z);
}

chunkIndex GetChunkIndex(float x, float y, float z)
{
	return chunkIndex(((int)x)/PATCH_CHUNK_SIZE,((int)y)/PATCH_CHUNK_SIZE,((int)z/PATCH_CHUNK_SIZE));
}

void WritePatchPoints(BigPatch *z, chunkIndex ci, const std::vector<gridPoint> &gridPoints)
{
    sprintf(z->location+z->locationRootLength,"/%d.%d.%d",std::get<2>(ci),std::get<1>(ci),std::get<0>(ci));
	
	FILE *f = fopen(z->location,"w+");
	
	if (f)
	{
	    for(const gridPoint &gp : gridPoints)
	    {
			fprintf(f,"%f,%f,%f,%f,%f\n",std::get<0>(gp),std::get<1>(gp),std::get<2>(gp),std::get<3>(gp),std::get<4>(gp));
	    }
	  
	  fclose(f);
	  
	  //printf("Loaded %d points\n",l);
	}
	else
	{
		//printf("Unable to open file %s\n",fname);
	}
}

void ReadPatchPoints(BigPatch *z, chunkIndex ci, std::vector<gridPoint> &gridPoints)
{
    sprintf(z->location+z->locationRootLength,"/%d.%d.%d",std::get<2>(ci),std::get<1>(ci),std::get<0>(ci));
	
	FILE *f = fopen(z->location,"r");

	float x,y;
	float xp,yp,zp;
	int l = 0;
	
	if (f)
	{
	    while (fscanf(f,"%f,%f,%f,%f,%f\n",&x,&y,&xp,&yp,&zp)==5)
	    {
			l++;
			gridPoints.push_back(gridPoint(x,y,xp,yp,zp));
	    }
	  
	  fclose(f);
	  
	  //printf("Loaded %d points\n",l);
	}
	else
	{
		//printf("Unable to open file %s\n",fname);
	}
}	

