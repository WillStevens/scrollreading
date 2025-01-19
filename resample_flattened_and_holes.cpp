#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <math.h>
#include <stdlib.h>

#include <unordered_set>
#include <vector>
#include <set>
#include <map>
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
weightedPointSet FindNearestPoints(float x, float y, float z, pointSet &flat, pointSet &target)
{
	weightedPointSet r;
	
	int i = 0;
	for(const std::tuple<float,float,float> &p : flat)
	{
		float xp = std::get<0>(p);
		float yp = std::get<1>(p);
		float zp = std::get<2>(p);
		
		float dist2 = (x-xp)*(x-xp)+(y-yp)*(y-yp)+(z-zp)*(z-zp);
		
		if (dist2 <= 1.1f*1.1f)
		{	
			float xt = std::get<0>(target[i]);
			float yt = std::get<1>(target[i]);
			float zt = std::get<2>(target[i]);
			r.push_back(std::tuple<float,float,float,float>(xt,yt,zt,exp(-dist2)));
		}
		
		i++;
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
	
	float xstep = (xmax-xmin)/(float)(int)(xmax-xmin);
	float zstep = (zmax-zmin)/(float)(int)(zmax-zmin);
	
	pointSet targetOutput,flatOutput,holeOutput;
	
	float xi,yi,zi;
	
	for(float x=xmin; x<=xmax; x+=xstep)
	{
		printf("At x-coord: %f\n",x);
		for(float z=zmin; z<=zmax; z+=zstep)
		{
			weightedPointSet p = FindNearestPoints(x,0,z,flatVolume,targetVolume);
			
			if (Interpolate(p,xi,yi,zi))
			{
				flatOutput.push_back( std::tuple<float,float,float>(x,0,z) );
				targetOutput.push_back( std::tuple<float,float,float>(xi,yi,zi) );
			}
			else
			{
				holeOutput.push_back( std::tuple<float,float,float>(x,0,z) );
			}
		}
		
		printf("target:%d, flat:%d, hole:%d\n",(int)targetOutput.size(),(int)flatOutput.size(),(int)holeOutput.size());
	}
	
	SaveVolume(argv[3],targetOutput);
	SaveVolume(argv[4],flatOutput);
	SaveVolume(argv[5],holeOutput);
}




