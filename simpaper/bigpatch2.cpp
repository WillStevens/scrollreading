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

typedef struct {
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

indexChunkIndex GetIndexChunkIndex(float x, float y)
{
	return indexChunkIndex(((int)x)/INDEX_CHUNK_SIZE,((int)y)/INDEX_CHUNK_SIZE);
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

std::vector<indexChunkIndex> GetAllIndexChunks(BigPatch *z)
{
	std::vector<indexChunkIndex> ret;
	DIR *dir;
	struct dirent *ent;

    sprintf(z->location+z->locationRootLength,"/index");
	
	if ((dir = opendir (z->location)) != NULL)
    {
		while ((ent = readdir (dir)) != NULL)
		{
            std::string file(ent->d_name);
			
		    if (file.length()>=5)
			{
				int x[2] = {0,0}, i=0;
			
				for(auto c : file)
				{
					if (c=='.')
						i++;
					else
						x[i]=x[i]*10 + c-'0';
				}
			
				ret.push_back(indexChunkIndex(x[1],x[0]));
			}
		}
	}
	
	return ret;
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

			for(unsigned i = 0; i<numPoints; i++)
			{
				gridPoints.push_back(gridPoint(bGridPoints[i].qx,
											   bGridPoints[i].qy,
											   bGridPoints[i].vx,
											   bGridPoints[i].vy,
											   bGridPoints[i].vz,
											   bGridPoints[i].patch));
			}
		}
	  
	    fclose(f);
	}
	else
	{
	}
}	

void WriteIndexedChunks(BigPatch *z, indexChunkIndex ci, const std::set<chunkIndex> &chunkIndices)
{
    sprintf(z->location+z->locationRootLength,"/index/%d.%d",std::get<1>(ci),std::get<0>(ci));
	
	FILE *f = fopen(z->location,"w");
	
	if (f)
	{
	    for(const chunkIndex &gci : chunkIndices)
	    {
			fprintf(f,"%d,%d,%d\n",std::get<0>(gci),std::get<1>(gci),std::get<2>(gci));
	    }
	  
	  fclose(f);
	  
	  //printf("Loaded %d points\n",l);
	}
	else
	{
		//printf("Unable to open file %s\n",fname);
	}
}

void ReadIndexedChunks(BigPatch *z, indexChunkIndex ci, std::set<chunkIndex> &chunkIndices)
{
    sprintf(z->location+z->locationRootLength,"/index/%d.%d",std::get<1>(ci),std::get<0>(ci));
	
	FILE *f = fopen(z->location,"r");

	int xi,yi,zi;
	int l = 0;
	
	if (f)
	{
	    while (fscanf(f,"%d,%d,%d\n",&xi,&yi,&zi)==3)
	    {
			l++;
			chunkIndices.insert(chunkIndex(xi,yi,zi));
	    }
	  
	  fclose(f);
	  
	  //printf("Loaded %d points\n",l);
	}
	else
	{
		//printf("Unable to open file %s\n",fname);
	}
}	

