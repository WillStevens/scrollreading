/*
This implements a way of representing surfaces that makes it efficient to check for overlap and
do alignment of patch and surface.

A surface is represented as a collection of patches, without bothering about removing patch intersections (because rendering will deal with that).

Each patch is a list of:
qx,qy,vx,vy,vz

where qx,qy are the floating point quadmesh coords within the patch and
vx,vy,vz are floating point scroll voxel coords.

A surface is a collection of patches represented as a list of:
qx,qy,vx,vy,vz,pid

For convenvience, points are added and compressed one patch at a time, but when using the data this cannot be assumed - points and patches
could be in any order within a chunk. For example, the boundary tracking routines will sometimes rewrite an entire chunk as a single
compressed block containing all patches within that chunk.

Where pid is the id of the patch that the point belongs to and qx,qy are the patch-local quadmesh coordinates

The list is stored in a compressed chunked format, chunked on the vx,vy,vz coords,
compressed using blosc2.

*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <blosc2.h>

#include <tuple>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <set>

typedef std::tuple<int,int,int> chunkIndex;
typedef std::tuple<int,int> indexChunkIndex;
typedef std::tuple<float,float,float,float,float,int> gridPoint;

typedef struct __attribute__((packed)) {
	float qx,qy;
	float vx,vy,vz;
	int patch;
} binaryGridPoint;

#define PATCH_CHUNK_SIZE 64
#define INDEX_CHUNK_SIZE 32

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

std::vector<chunkIndex> GetAllPatchChunks(BigPatch *z)
{
	std::vector<chunkIndex> ret;
	DIR *dir;
	struct dirent *ent;

    sprintf(z->location+z->locationRootLength,"/surface");
	
	if ((dir = opendir (z->location)) != NULL)
    {
		while ((ent = readdir (dir)) != NULL)
		{
            std::string file(ent->d_name);
			
		    if (file.length()>=5)
			{
				int x[3] = {0,0,0}, i=0;
			
				for(auto c : file)
				{
					if (c=='.')
						i++;
					else
						x[i]=x[i]*10 + c-'0';
				}
			
				ret.push_back(chunkIndex(x[2],x[1],x[0]));
			}
		}
	}
	
	return ret;
}

void EraseChunk(BigPatch *z,chunkIndex ci)
{
    sprintf(z->location+z->locationRootLength,"/surface/%d.%d.%d",std::get<2>(ci),std::get<1>(ci),std::get<0>(ci));

	//printf("Trying to remove:%s\n",z->location);
	remove(z->location);
}

// TODO - this could be done more efficiently by passing in the binary data as a parameter rather than using a vector
// This will be done later after the function is confirmed working correctly
void WritePatchPoints(BigPatch *z, chunkIndex ci, const std::vector<gridPoint> &gridPoints)
{	
    sprintf(z->location+z->locationRootLength,"/surface/%d.%d.%d",std::get<2>(ci),std::get<1>(ci),std::get<0>(ci));
	
	FILE *f = fopen(z->location,"a");
	
	if (f)
	{
		binaryGridPoint *bGridPoints = (binaryGridPoint *)malloc(sizeof(binaryGridPoint)*gridPoints.size());
        uint8_t *compressedData = (uint8_t *)malloc(sizeof(binaryGridPoint)*gridPoints.size()+BLOSC2_MAX_OVERHEAD);
		uint32_t i = 0;
	    for(const gridPoint &gp : gridPoints)
	    {
			bGridPoints[i].qx = std::get<0>(gp);
			bGridPoints[i].qy = std::get<1>(gp);
			bGridPoints[i].vx = std::get<2>(gp);
			bGridPoints[i].vy = std::get<3>(gp);
			bGridPoints[i].vz = std::get<4>(gp);
			bGridPoints[i].patch = std::get<5>(gp);
			i++;
	    }
	    // Now i == gridPoints.size()
		
	    blosc1_set_compressor("zstd");
	    int compressed_len = blosc2_compress(3,1,sizeof(binaryGridPoint),bGridPoints,sizeof(binaryGridPoint)*gridPoints.size(),compressedData,sizeof(binaryGridPoint)*gridPoints.size()+BLOSC2_MAX_OVERHEAD);

        if (compressed_len <= 0) {
		  fprintf(stderr,"Compression error in " __FILE__);
          exit(-1);
        }

		fwrite(&i,1,sizeof(uint32_t),f);
		fwrite(&compressed_len,1,sizeof(int),f);
	    fwrite(compressedData,1,compressed_len,f);

		free(compressedData);
		free(bGridPoints);
	  
	    fclose(f);
	}
	else
	{
		printf("Unable to open %s to append\n",z->location);
	}
}

void ReadPatchPoints(BigPatch *z, chunkIndex ci, std::vector<gridPoint> &gridPoints)
{
    sprintf(z->location+z->locationRootLength,"/surface/%d.%d.%d",std::get<2>(ci),std::get<1>(ci),std::get<0>(ci));
	
	FILE *f = fopen(z->location,"r");
	
	if (f)
	{
		uint32_t numPoints;
		int compressed_len;
		// Keep going until we fail to read the first item of the record : the number of points
		while(true)
		{
			if (fread(&numPoints,1,sizeof(uint32_t),f)<sizeof(uint32_t))
				break;
			fread(&compressed_len,1,sizeof(int),f);

			binaryGridPoint *bGridPoints = (binaryGridPoint *)malloc(sizeof(binaryGridPoint)*numPoints);
			uint8_t *compressedData = (uint8_t *)malloc(sizeof(binaryGridPoint)*numPoints+BLOSC2_MAX_OVERHEAD);
		
		    fread(compressedData,1,compressed_len,f);
					
			blosc1_set_compressor("zstd");
			int decompressed_size = blosc2_decompress(compressedData, compressed_len, bGridPoints, sizeof(binaryGridPoint)*numPoints);
            if (decompressed_size < 0) {
			    fprintf(stderr,"Decompression error in " __FILE__);
                exit(-1);
            }

			size_t currentGridPointsSize = gridPoints.size();
			gridPoints.resize(currentGridPointsSize+numPoints);
			
			for(unsigned i = 0; i<numPoints; i++)
			{
				float qx,qy,vx,vy,vz;
				int patch;
				
				qx = bGridPoints[i].qx;
	            qy = bGridPoints[i].qy;
				vx = bGridPoints[i].vx;
				vy = bGridPoints[i].vy;
				vz = bGridPoints[i].vz;
				patch = bGridPoints[i].patch;
				
				gridPoints[currentGridPointsSize+i] = gridPoint(qx,qy,vx,vy,vz,patch);
			}
			
			free(compressedData);
			free(bGridPoints);
		}
	  
	    fclose(f);
	}
	else
	{
	}
}	

/* Pick a random point by first picking a random chunk, then a random point within that chunk */
/* Not every point has an equal chance of being picked, because this doesn't take account of number of points in a chunk */
/* But for some purposes this doesn't matter */
/* This function was written for select a new seed from a boundary */
/* rn1,rn2 are randomly generated numbers to use for selecting the point */
gridPoint SelectRandomPoint(BigPatch *z, unsigned rn1, unsigned rn2)
{
	std::vector<chunkIndex> allChunks = GetAllPatchChunks(z);
	
	int n = allChunks.size();
	
	std::vector<gridPoint> gridPoints;

//    printf("Chosen chunk %d,%d,%d\n",std::get<0>(allChunks[rn1%n]),std::get<1>(allChunks[rn1%n]),std::get<1>(allChunks[rn1%n]));
	
	ReadPatchPoints(z,allChunks[rn1%n],gridPoints);
	
	n = gridPoints.size();

//    printf("Chosen point %d of %d in chunk:%f %f %f\n",rn2%n,n,std::get<2>(gridPoints[rn2%n]),std::get<3>(gridPoints[rn2%n]),std::get<4>(gridPoints[rn2%n]));	
	return gridPoints[rn2%n];
}