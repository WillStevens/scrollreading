#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <tuple>

#include "bigpatch.cpp"

typedef std::tuple<float,float,float> point;
typedef std::vector< point > pointVector;

void DilateChunkIndices(std::set<chunkIndex> &chunkIndices)
{
	std::set<chunkIndex> toAdd;
	for(auto &ci : chunkIndices)
	{
		for(int xo=-1; xo<=1; xo++)
		for(int yo=-1; yo<=1; yo++)
		for(int zo=-1; zo<=1; zo++)
			toAdd.insert(chunkIndex(std::get<0>(ci)+xo,std::get<1>(ci)+yo,std::get<2>(ci)+zo));
	}
	
	chunkIndices.insert(toAdd.begin(),toAdd.end());
}

int main(int argc,char *argv[])
{
	if (argc != 5)
	{
		fprintf(stderr,"Usage: find_nearest2 <bigpatch> x y r\n");
		fprintf(stderr,"Find all of the grid points within a radius r of x,y, write them to stdout.\n");
		fprintf(stderr,"Radius can't exceed chunk size\n");
		exit(-1);
	}
	
	float xTarg = atof(argv[2]);
	float yTarg = atof(argv[3]);
	float radius2 = atof(argv[4]);
    radius2=radius2*radius2; // square of the radius
	
    float xmin=0,ymin=0,zmin=0,minR2=0;

	pointVector pv;
	
	BigPatch *bp = OpenBigPatch(argv[1]);

	if(bp)
	{
		indexChunkIndex ci = GetIndexChunkIndex(xTarg,yTarg);
		
		std::set<chunkIndex> chunkIndices;
		ReadIndexedChunks(bp,ci,chunkIndices);
		
		DilateChunkIndices(chunkIndices);
		
		bool first = true;
		float x,y,xp,yp,zp,r2;

		for(auto const &i : chunkIndices)
		{
			//printf("Chunk: %d.%d.%d\n",std::get<2>(i),std::get<1>(i),std::get<0>(i));
			std::vector<gridPoint> gridPoints;
			ReadPatchPoints(bp,i,gridPoints);
			for(auto const &gp : gridPoints)
			{
				x = std::get<0>(gp);
				y = std::get<1>(gp);
				xp = std::get<2>(gp);
				yp = std::get<3>(gp);
				zp = std::get<4>(gp);
						
				r2 = (x-xTarg)*(x-xTarg)+(y-yTarg)*(y-yTarg);
		
				if (r2 < radius2)
				{
					if (r2<minR2 || first)
					{
						xmin=xp;
						ymin=yp;
						zmin=zp;
						minR2=r2;
						first = false;
					}
			  
					pv.push_back(point(xp,yp,zp));
				}
			}
		}
		
		CloseBigPatch(bp);
	}

	// print the nearest first
	printf("%f,%f,%f\n",xmin,ymin,zmin);
	
	for(const point &p : pv)
	{
		float xp = std::get<0>(p);
		float yp = std::get<1>(p);
		float zp = std::get<2>(p);
		
		if (xp != xmin || yp != ymin || zp != zmin)
		  printf("%f,%f,%f\n",xp,yp,zp);
	}
		
	return 0;
}
