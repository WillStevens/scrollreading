// Given two patches, work out the best rotatation and offset for the second one to align it with the first
#include <stdio.h>
#include <math.h>

#include <tuple>
#include <vector>
#include <map>
#include <algorithm>
#include <set>

#include "bigpatch.cpp"

typedef std::tuple<int,int,int,int,float> match;

#define CELL_SIZE 10
typedef std::tuple<int,int,int> gridCell;

#define SHEET_SIZE_X 3000
#define SHEET_SIZE_Y 6000

std::vector<gridPoint> gridPoints[2];

std::map< gridCell, std::vector<gridPoint> > cellMap0;
std::map< gridCell, std::vector<gridPoint> > cellMap1;

typedef std::map< gridCell, std::vector<gridPoint> >::iterator cellMapIterator;

typedef std::tuple<float,float,float,float,float,float> affineTx;

//#define DEBUG

affineTx AffineTxMultiply(const affineTx &m, const affineTx &n)
{
	/* a,b,c,d,e,f are the coeffs of the transform
	   (Yx) = ( a b c ) (Xx)
	   (Yy)   ( d e f ) (Xy)
	   (1)    ( 0 0 1 ) (1)
	*/
	
	float a = std::get<0>(m);
	float b = std::get<1>(m);
	float c = std::get<2>(m);
	float d = std::get<3>(m);
	float e = std::get<4>(m);
	float f = std::get<5>(m);

	float ad = std::get<0>(n);
	float bd = std::get<1>(n);
	float cd = std::get<2>(n);
	float dd = std::get<3>(n);
	float ed = std::get<4>(n);
	float fd = std::get<5>(n);
	
	return std::tuple(a*ad+b*dd,a*bd+b*ed,a*cd+b*fd+c,d*ad+e*dd,d*bd+e*ed,d*cd+e*fd+f);
}

float Distance(float x0, float y0, float z0, float x1, float y1, float z1)
{
	float dx = x1-x0, dy = y1-y0, dz = z1-z0;
	
	float d2 = dx*dx+dy*dy+dz*dz;
	
	return sqrt(d2);
}

float Distance(float x0, float y0, float x1, float y1)
{
	return Distance(x0,y0,0,x1,y1,0);
}


void LoadPointSet(const char *fname, int index)
{
	FILE *f = fopen(fname,"r");

	int l = 0;
		
	float x,y;
	float xp,yp,zp;
	
	if (f)
	{
	    while (fscanf(f,"%f,%f,%f,%f,%f\n",&x,&y,&xp,&yp,&zp)==5)
	    {
		  l++;
		  
		  gridPoints[index].push_back(gridPoint(x,y,xp,yp,zp));
	    }
	  
	  fclose(f);
	  
	  //printf("Loaded %d points\n",l);
	}
	else
	{
		//printf("Unable to open file %s\n",fname);
	}
}

void FillCellMap(void)
{
	for(auto const &gp : gridPoints[0])
	{
	  int xc = ((int)std::get<2>(gp))/CELL_SIZE;
	  int yc = ((int)std::get<3>(gp))/CELL_SIZE;
	  int zc = ((int)std::get<4>(gp))/CELL_SIZE;

      gridCell g(xc,yc,zc);

      if (cellMap0.count(g)==0)
        cellMap0[g] = std::vector<gridPoint>();			  
      cellMap0[g].push_back(gp);
	}

	for(auto const &gp : gridPoints[1])
	{
	  int xc = ((int)std::get<2>(gp))/CELL_SIZE;
	  int yc = ((int)std::get<3>(gp))/CELL_SIZE;
	  int zc = ((int)std::get<4>(gp))/CELL_SIZE;

      gridCell g(xc,yc,zc);

      if (cellMap1.count(g)==0)
        cellMap1[g] = std::vector<gridPoint>();			  
      cellMap1[g].push_back(gp);
	}
}

void FindMatches(std::vector<match> &matchList)
{
#ifdef DEBUG
	printf("Iterating over points\n");
#endif
	for(cellMapIterator i = cellMap0.begin(); i!= cellMap0.end(); i++)
	{
		int g0x = std::get<0>(i->first);
		int g0y = std::get<1>(i->first);
		int g0z = std::get<2>(i->first);
				
		bool foundAny = false;
		for(int nx = g0x-1; nx<=g0x+1; nx++)
		for(int ny = g0y-1; ny<=g0y+1; ny++)
		for(int nz = g0z-1; nz<=g0z+1; nz++)
		{
			gridCell g(nx,ny,nz);
			if (cellMap1.count(g) != 0)
			{
				foundAny = true;
			}
		}
		
		if (foundAny)
		{
		    for(const gridPoint &gp0 : cellMap0[i->first])
			{
		        float x0 = std::get<0>(gp0);
		        float y0 = std::get<1>(gp0);
		        float xp0 = std::get<2>(gp0);
		        float yp0 = std::get<3>(gp0);
		        float zp0 = std::get<4>(gp0);

				float minDist = 10000;
	            int minx1=-1,miny1=-1;

				for(int nx = g0x-1; nx<=g0x+1; nx++)
				for(int ny = g0y-1; ny<=g0y+1; ny++)
				for(int nz = g0z-1; nz<=g0z+1; nz++)
		        {
					gridCell g(nx,ny,nz);
			        if (cellMap1.count(g) != 0)
			        {		
					    for(const gridPoint &gp1 : cellMap1[g])
			            {		  
		                    float xp1 = std::get<2>(gp1);
		                    float yp1 = std::get<3>(gp1);
		                    float zp1 = std::get<4>(gp1);
		  
   		                    float d = Distance(xp0,yp0,zp0,xp1,yp1,zp1);
		                    if (d<minDist)
		                    {
			                    minDist = d;
			                    minx1 = std::get<0>(gp1); miny1 = std::get<1>(gp1);
		                    }
			            }
			        }
		        }
				
 			    if (minDist<5.0)
	            {
		            matchList.push_back(match(x0,y0,minx1,miny1,minDist));		  
	            }
			}
		}
	}
	
    // Now we want to prune the list so that each x1 y1 appears only once, with minimum d
	std::vector< std::vector<match>::iterator > toDelete;
	
	// Sort matchlist by elements 2 and 3 then 4
    auto sortMatchlistLambda = [] (match const& m1, match const& m2) -> bool
    {
       return std::get<2>(m1) < std::get<2>(m2) || 
	          (std::get<2>(m1) == std::get<2>(m2) && std::get<3>(m1) < std::get<3>(m2)) ||
			  (std::get<2>(m1) == std::get<2>(m2) && std::get<3>(m1) == std::get<3>(m2) && std::get<4>(m1) < std::get<4>(m2));
    };

#ifdef DEBUG
	printf("Sorting matchlist\n");
#endif
    
    std::sort(matchList.begin(), matchList.end(), sortMatchlistLambda);  

#ifdef DEBUG
	printf("Iterating over matchlist\n");
#endif
	
	// For all x1,y1 in matchlist (elements 2 and 3) remove all except the first occuring (the shortest distance)
	for(std::vector<match>::iterator ma = matchList.begin(); ma != matchList.end();)
	{
		int ax1 = std::get<2>(*ma);
		int ay1 = std::get<3>(*ma);
		
		for(ma = ma+1; ma != matchList.end(); ma++)
		{
			int bx1 = std::get<2>(*ma);
			int by1 = std::get<3>(*ma);
			
			if (ax1==bx1 && ay1==by1)
			{
				toDelete.push_back(ma);
			}
			else
			{
				break;
			}
		}
	}
	
	for(const std::vector<match>::iterator &d : toDelete)
	{
		matchList.erase(d);
	}
	
#ifdef DEBUG
	printf("Returning from FindMatches\n");
#endif	
}

void AlignMatches(std::vector<match> &matchList)
{
	// Pick two random points from matchList
	// Check the distances of both pairs.
	// If they agree approximately, and the distance is large enough
	// Then calculate the necessary transformation
	
	// Build up several transforms
	// If one patch is flipped then the transforms will all be different from each other
	// If patches aren't flipped then they will all be similar
	// Assess this by working out the standard deviation of each part of the tranform
	// Make sure we sample at least 10
	
	int l = matchList.size();
	
	std::vector<affineTx> transformSamples;
	
	for(int i = 0; l>0 && i<100; i++)
	{
		int pa = rand()%l;
		int pb = rand()%l;
		
		float ax0 = std::get<0>(matchList[pa]);
		float ay0 = std::get<1>(matchList[pa]);
		float ax1 = std::get<2>(matchList[pa]);
		float ay1 = std::get<3>(matchList[pa]);

		float bx0 = std::get<0>(matchList[pb]);
		float by0 = std::get<1>(matchList[pb]);
		float bx1 = std::get<2>(matchList[pb]);
		float by1 = std::get<3>(matchList[pb]);
		
		float d0 = Distance(ax0,ay0,bx0,by0);
		float d1 = Distance(ax1,ay1,bx1,by1);
		
		//printf("distance 0: %f, distance 1: %f\n",d0,d1);
	
	    if (d1>100.0 && d0/d1>0.98 && d0/d1<1.02)
		{						
			// Now work out the angle
			float dx1 = bx1-ax1, dy1 = by1-ay1;
			float dx0 = bx0-ax0, dy0 = by0-ay0; 
			
			float theta = atan2(dy1,dx1)-atan2(dy0,dx0);
			
			// The transformation we need to output is:
			// Translation of ax1 and ay1 to origin
			affineTx t1 = std::tuple(1,0,ax0,0,1,ay0);
			// Rotate by theta
			affineTx r1 = std::tuple(cos(-theta),-sin(-theta),0,sin(-theta),cos(-theta),0);
			// Translation of origin back to ax0, ay0
			affineTx t2 = std::tuple(1,0,-ax1,0,1,-ay1);
/*
			affineTx t1 = std::tuple(1,0,ax1,0,1,ay1);
			// Rotate by theta
			affineTx r1 = std::tuple(cos(theta),-sin(theta),0,sin(theta),cos(theta),0);
			// Translation of origin back to ax0, ay0
			affineTx t2 = std::tuple(1,0,-ax0,0,1,-ay0);
*/		
			affineTx affineTxOut = AffineTxMultiply(t1,AffineTxMultiply(r1,t2));
			
			//printf("ax1 = %f, ay1 = %f, ax0 = %f, ay0 = %f, theta = %f\n",ax1,ay1,ax0,ay0,theta);

			transformSamples.push_back(affineTxOut);
			
			//exit(0);
		}
	}
	
	if (l==0)
	{
		//printf("No matches to align\n");
	}
	else if (transformSamples.size()<10)
	{
		// Not enough transforms to get SD
	}
	else
	{
		affineTx sampleVariance = affineTx(0,0,0,0,0,0);
	    affineTx sampleMean = affineTx(0,0,0,0,0,0); 
		
		for(const affineTx &tx : transformSamples)
		{
			sampleMean = affineTx(std::get<0>(sampleMean)+std::get<0>(tx),
			                      std::get<1>(sampleMean)+std::get<1>(tx),
			                      std::get<2>(sampleMean)+std::get<2>(tx),
			                      std::get<3>(sampleMean)+std::get<3>(tx),
			                      std::get<4>(sampleMean)+std::get<4>(tx),
			                      std::get<5>(sampleMean)+std::get<5>(tx)
			                      );
		}
		sampleMean = affineTx(std::get<0>(sampleMean)/transformSamples.size(),
			                  std::get<1>(sampleMean)/transformSamples.size(),
			                  std::get<2>(sampleMean)/transformSamples.size(),
			                  std::get<3>(sampleMean)/transformSamples.size(),
			                  std::get<4>(sampleMean)/transformSamples.size(),
			                  std::get<5>(sampleMean)/transformSamples.size()
			                  );

		
		float minDistanceFromMean = 0;
		int i = 0;
		int mini = 0;
		for(const affineTx &tx : transformSamples)
		{
			// See how far the transformation part if from the mean
			float distanceFromMean = pow(std::get<3>(tx)-std::get<3>(sampleMean),2) + pow(std::get<5>(tx)-std::get<5>(sampleMean),2);
			
			sampleVariance = affineTx(std::get<0>(sampleVariance)+pow(std::get<0>(tx)-std::get<0>(sampleMean),2),
			                          std::get<1>(sampleVariance)+pow(std::get<1>(tx)-std::get<1>(sampleMean),2),
			                          std::get<2>(sampleVariance)+pow(std::get<2>(tx)-std::get<2>(sampleMean),2),
			                          std::get<3>(sampleVariance)+pow(std::get<3>(tx)-std::get<3>(sampleMean),2),
			                          std::get<4>(sampleVariance)+pow(std::get<4>(tx)-std::get<4>(sampleMean),2),
			                          std::get<5>(sampleVariance)+pow(std::get<5>(tx)-std::get<5>(sampleMean),2)
			                          );
			if (i==0 || distanceFromMean < minDistanceFromMean)
			{
				mini = i;
				minDistanceFromMean = distanceFromMean;
			}				
			
		}
		
		sampleVariance = affineTx(std::get<0>(sampleVariance)/transformSamples.size(),
			                      std::get<1>(sampleVariance)/transformSamples.size(),
			                      std::get<2>(sampleVariance)/transformSamples.size(),
			                      std::get<3>(sampleVariance)/transformSamples.size(),
			                      std::get<4>(sampleVariance)/transformSamples.size(),
			                      std::get<5>(sampleVariance)/transformSamples.size()
			                  );
		
		printf("%f %f %f %f %f %f ",
			  std::get<0>(sampleVariance),
			  std::get<1>(sampleVariance),
			  std::get<2>(sampleVariance),
			  std::get<3>(sampleVariance),
			  std::get<4>(sampleVariance),
			  std::get<5>(sampleVariance)
			  );

  		printf("%f %f %f %f %f %f\n",
			  std::get<0>(transformSamples[mini]),
			  std::get<1>(transformSamples[mini]),
			  std::get<2>(transformSamples[mini]),
			  std::get<3>(transformSamples[mini]),
			  std::get<4>(transformSamples[mini]),
			  std::get<5>(transformSamples[mini])
			  );
        exit(0);
		//printf("Matches found\n");
	}
	
	exit(1);
}

int main(int argc, char * argv[])
{
	if (argc != 3)
	{
		printf("Usage: align_patches3 <patch 1> <patch 2>\n");
		printf("Patch 1 is a bigpatch, patch 2 is a CSV patch\n");
		exit(-1);
	}
	
#ifdef DEBUG
	printf("Load 2\n");
#endif
	LoadPointSet(argv[2],1);
	
	
#ifdef DEBUG
	printf("Load 1\n");
#endif

    BigPatch *bp = OpenBigPatch(argv[1]);
	
	if (!bp)
	{
		printf("Failed to open bigpatch:%s\n",argv[1]);
		exit(-2);
	}
	
	std::set<chunkIndex> chunks;
	
	for(const gridPoint &gp : gridPoints[1])
	{
		// Which chunk is the point in?
		chunks.insert(GetChunkIndex(std::get<0>(gp),std::get<1>(gp),std::get<2>(gp)));
	}

	std::set<chunkIndex> expanded;
	for(const chunkIndex &chunk : chunks)
	{
		for(int xo=-1; xo<=1; xo++)
		for(int yo=-1; yo<=1; yo++)
		for(int zo=-1; zo<=1; zo++)
		{
			expanded.insert(chunkIndex(std::get<0>(chunk)+xo,std::get<1>(chunk)+yo,std::get<2>(chunk)+zo));
		}
	}
	
	chunks.insert(expanded.begin(),expanded.end());

	for(const chunkIndex &chunk : chunks)
	{
		ReadPatchPoints(bp,chunk,gridPoints[0]);
	}
	
	CloseBigPatch(bp);
	
    FillCellMap();
	
	std::vector<match> matchList;
	
	FindMatches(matchList);
	
	AlignMatches(matchList);
	
	exit(2);
}