#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <math.h>
#include <stdlib.h>

#include <unordered_set>
#include <vector>
#include <set>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <utility>

typedef std::vector<std::tuple<float,float,float> > pointSet;
typedef std::vector<std::tuple<float,float,float,float> > weightedPointSet;

pointSet LoadVolume(const char *file, float &xmin, float &xmax, float &zmin, float &zmax)
{
	float x,y,z;
	
	xmin = zmin = 1000000.0f;
	xmax = zmax = -1000000.0f;
	
    pointSet volume;
	
	FILE *f = fopen(file,"r");
	
	while(f)
	{
		if (fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
		{
		  volume.push_back( std::tuple<float,float,float>(x,y,z) );
		  
		  if (x<xmin) xmin = x;
		  if (x>xmax) xmax = x;
		  if (z<zmin) zmin = z;
		  if (z>zmax) zmax = z;
		}
		else
			break;
	}
	
	fclose(f);
	
	return std::move(volume);
}

void SaveVolume(const char *file, pointSet &ps)
{
	FILE *f = fopen(file,"w");
	
	if(f)
	{
		for(const std::tuple<float,float,float> &p : ps)
		{
			float xp = std::get<0>(p);
			float yp = std::get<1>(p);
			float zp = std::get<2>(p);
			
			fprintf(f,"%f,%f,%f\n",xp,yp,zp);
		}
		
		fclose(f);
	}
}

// Points have a good span if at least two of them have an angle of > 120
bool GoodSpan(weightedPointSet &wp)
{
	return true;
}

// returns an unnormalised weighted point set for the target point
// could do this much more efficiently is flat is sorted.
weightedPointSet FindNearestPoints(float x, float y, float z, pointSet &flat, pointSet &target, std::vector<int> &sortedIndex)
{
	weightedPointSet r;
	float neighbourDistance = 1.2f;
    float neighbourDistanceSquared = neighbourDistance*neighbourDistance;
	
	// sortedIndex is sorted by x-coord of flat, and indices of flat and target are in correspondance

	// make sure we only test about 1/500th of the points
	int step = sortedIndex.size()/500;
	int starti = 0;
	int endi = sortedIndex.size();
	for(int i = 1; i<sortedIndex.size(); i+= step)
	{
		if (std::get<0>(flat[sortedIndex[i]]) < x-neighbourDistance)
			starti = i;
		if (std::get<0>(flat[sortedIndex[sortedIndex.size()-i]]) > x+neighbourDistance)
			endi = flat.size()-i;
	}
	
	for(int i = starti; i<endi; i++)
	{
		float xp = std::get<0>(flat[sortedIndex[i]]);
		
		if ((x-xp)*(x-xp) > neighbourDistanceSquared)
			continue;
		
		float yp = std::get<1>(flat[sortedIndex[i]]);
		float zp = std::get<2>(flat[sortedIndex[i]]);
				
		float dist2 = (x-xp)*(x-xp)+(y-yp)*(y-yp)+(z-zp)*(z-zp);
		
		if (dist2 <= neighbourDistanceSquared)
		{	
			float xt = std::get<0>(target[sortedIndex[i]]);
			float yt = std::get<1>(target[sortedIndex[i]]);
			float zt = std::get<2>(target[sortedIndex[i]]);
			r.push_back(std::tuple<float,float,float,float>(xt,yt,zt,exp(-dist2)));
		}
	}
	
	if (GoodSpan(r))
		return std::move(r);
	else
		return weightedPointSet();
}

bool Interpolate(weightedPointSet &wp, float &xi, float &yi, float &zi)
{
	if (wp.size()==0)
		return false;
	
	float totalWeight = 0;
	xi = yi = zi = 0;
	
	for(const std::tuple<float,float,float,float> &p : wp)
	{
		float xp = std::get<0>(p);
		float yp = std::get<1>(p);
		float zp = std::get<2>(p);
		float w = std::get<3>(p);
	
		xi += xp*w;
		yi += yp*w;
		zi += zp*w;
		
		totalWeight += w;
	}
	
	xi /= totalWeight;
	yi /= totalWeight;
	zi /= totalWeight;
	
	return true;
}

int main(int argc, char *argv[])
{
	if (argc != 6)
	{
		printf("Usage: resample_flattened_and_holes <target-points> <flattened-points> <target-output> <flat-output> <hole-ouput>\n");
		return -1;
	}
	
	float xmin,xmax,zmin,zmax;
	
	printf("Loading flattened points...\n");

	// There is a 1:1 correspondance between points in the target and flat volume
	pointSet targetVolume = LoadVolume(argv[1],xmin,xmax,zmin,zmax);
	pointSet flatVolume = LoadVolume(argv[2],xmin,xmax,zmin,zmax);
    
	printf("Loaded %d flattened points\n",(int)targetVolume.size());

	// Make an index array to use for sorting
	std::vector<int> sortedIndex(flatVolume.size(),0);
	for(int i = 0; i!=sortedIndex.size(); i++)
		sortedIndex[i]=i;

    std::sort(sortedIndex.begin(), sortedIndex.end(), 
	  [&] (const int& a, const int& b)
      {
         return std::get<0>(flatVolume[a]) < std::get<0>(flatVolume[b]);
      }
	);
    
		
	float xstep = (xmax-xmin)/(float)(int)(xmax-xmin);
	float zstep = (zmax-zmin)/(float)(int)(zmax-zmin);
	
	pointSet targetOutput,flatOutput,holeOutput,holeOutputBuffer;
	std::unordered_map<float,float> xminByZ;
	std::unordered_map<float,float> xmaxByZ;
	std::unordered_map<float,float> zminByX;
	std::unordered_map<float,float> zmaxByX;
	
	float xi,yi,zi;
	
	for(float x=xmin; x<=xmax; x+=xstep)
	{
		printf("At x-coord: %f\n",x);
		
		float zminByX_tmp = zmax;
		float zmaxByX_tmp = zmin;
		
		for(float z=zmin; z<=zmax; z+=zstep)
		{
			weightedPointSet p = FindNearestPoints(x,0,z,flatVolume,targetVolume,sortedIndex);
						
			if (Interpolate(p,xi,yi,zi))
			{
				flatOutput.push_back( std::tuple<float,float,float>(x,0,z) );
				targetOutput.push_back( std::tuple<float,float,float>(xi,yi,zi) );
				
				if (xminByZ.count(z)==0 || x<xminByZ[z])
					xminByZ[z]=x;
				if (xmaxByZ.count(z)==0 || x>xmaxByZ[z])
					xmaxByZ[z]=x;
				if (z<zminByX_tmp)
					zminByX_tmp=z;
				if (z>zmaxByX_tmp)
					zmaxByX_tmp=z;
			}
			else
			{
				holeOutputBuffer.push_back( std::tuple<float,float,float>(x,0,z) );
			}
		}

		zminByX[x]=zminByX_tmp;
		zmaxByX[x]=zmaxByX_tmp;;
		
		printf("target:%d, flat:%d, hole:%d\n",(int)targetOutput.size(),(int)flatOutput.size(),(int)holeOutputBuffer.size());
	}
	
	for(const std::tuple<float,float,float> &p : holeOutputBuffer)
	{
		float x = std::get<0>(p);
		float z = std::get<2>(p);
		
		if (x>=xminByZ[z] && x<=xmaxByZ[z] || z>=zminByX[x] && z<=zmaxByX[x])
		{
			holeOutput.push_back(p);
		}			
	}
	
	SaveVolume(argv[3],targetOutput);
	SaveVolume(argv[4],flatOutput);
	SaveVolume(argv[5],holeOutput);
}




