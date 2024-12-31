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

#define PROJECTION_SIZE 2048
#define PROJECTION_BLANK -10000.0f
float projection[PROJECTION_SIZE][PROJECTION_SIZE][3];

void InitializePrjection(void)
{
	for(int x=0;x<PROJECTION_SIZE;x++)
		for(int y=0;y<PROJECTION_SIZE;y++)
			for(int k=0;k<3;k++)
				projection[x][y][k]=PROJECTION_BLANK;
		
}

pointSet LoadVolume(const char *file)
{
	float x,y,z;
	
    pointSet volume;
	
	FILE *f = fopen(file,"r");
	
	while(f)
	{
		if (fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
		{
		  volume.push_back( std::tuple<float,float,float>(x,z,y) );
		}
		else
			break;
	}
	
	fclose(f);
	
	return std::move(volume);
}

void ProjectVolume(pointSet &volume, int &xmin, int &xmax, int &ymin, int &ymax)
{
    float x,y,z;
	
	xmin = PROJECTION_SIZE-1;
	ymin = PROJECTION_SIZE-1;
	xmax = 0;
	ymax = 0;
	
	for(const std::tuple<float,float,float> &p : volume)
	{
		x = std::get<0>(p);
		y = std::get<1>(p);
		z = std::get<2>(p);
		
		projection[(int)x+PROJECTION_SIZE/2][(int)y+PROJECTION_SIZE/2][0]=x;
		projection[(int)x+PROJECTION_SIZE/2][(int)y+PROJECTION_SIZE/2][1]=y;
		
		if (x+PROJECTION_SIZE/2 < xmin)
			xmin = x+PROJECTION_SIZE/2;
		if (x+PROJECTION_SIZE/2 > xmax)
			xmax = x+PROJECTION_SIZE/2;
		if (y+PROJECTION_SIZE/2 < ymin)
			ymin = y+PROJECTION_SIZE/2;
		if (y+PROJECTION_SIZE/2 > ymax)
			ymax = y+PROJECTION_SIZE/2;
		
		if (z<projection[(int)x+PROJECTION_SIZE/2][(int)y+PROJECTION_SIZE/2][2] || projection[(int)x+PROJECTION_SIZE/2][(int)y+PROJECTION_SIZE/2][2]==PROJECTION_BLANK)
			projection[(int)x+PROJECTION_SIZE/2][(int)y+PROJECTION_SIZE/2][2]=z;
	}
}

bool InterpolatePoint(int x, int y, int xmin, int xmax, int ymin, int ymax, float &xi, float &yi, float &zi)
{
	
	xi = 0.0f;
	yi = 0.0f;
	zi = 0.0f;
	
	int32_t yminus = INT32_MIN;
	int32_t yplus = INT32_MIN;
	int32_t xminus = INT32_MIN;
	int32_t xplus = INT32_MIN;
	
	// Look for first non-zero point in -y direction
	for(int yo = y; yo>=ymin; yo--)
	  if (projection[x][yo][0] != PROJECTION_BLANK)
	  {
		  yminus = yo;
		  break;
	  }
	// Look for first non-zero point in +y direction
	for(int yo = y; yo<=ymax; yo++)
	  if (projection[x][yo][0] != PROJECTION_BLANK)
	  {
		  yplus = yo;
		  break;
	  }
	// Look for first non-zero point in -x direction
	for(int xo = x; xo>=xmin; xo--)
	  if (projection[xo][y][0] != PROJECTION_BLANK)
	  {
		  xminus = xo;
		  break;
	  }
	// Look for first non-zero point in +x direction
	for(int xo = x; xo<=xmax; xo++)
	  if (projection[xo][y][0] != PROJECTION_BLANK)
	  {
		  xplus = xo;
		  break;
	  }
	
	printf("In InterpolatePoint for %d,%d : %d,%d,%d,%d\n",x,y,xminus,xplus,yminus,yplus);
	
	if (xminus != INT32_MIN && xplus != INT32_MIN && (yminus==INT32_MIN || yplus==INT32_MIN || yplus-yminus > xplus-xminus))
	{
		float wplus = (1000.0*(x-xminus))/(xplus - xminus);
		float wminus = 1000.0-wplus;
		
		
		xi = (projection[xminus][y][0]*wminus + projection[xplus][y][0]*wplus)/1000;
		yi = (projection[xminus][y][1]*wminus + projection[xplus][y][1]*wplus)/1000;
		zi = (projection[xminus][y][2]*wminus + projection[xplus][y][2]*wplus)/1000;	

		printf("A:%f,%f,%f\n",xi,yi,zi);
		
        return true;		
	}
    else if (yminus != INT32_MIN && yplus!=INT32_MIN)
	{
		float wplus = (1000.0*(y-yminus))/(yplus - yminus);
		float wminus = 1000.0-wplus;
		
		xi = (projection[x][yminus][0]*wminus + projection[x][yplus][0]*wplus)/1000;
		yi = (projection[x][yminus][1]*wminus + projection[x][yplus][1]*wplus)/1000;
		zi = (projection[x][yminus][2]*wminus + projection[x][yplus][2]*wplus)/1000;

		printf("B:%f,%f,%f\n",xi,yi,zi);
		
		return true;
	}		
	
	return false;
}

pointSet Interpolate(int xmin,int xmax,int ymin,int ymax)
{
	pointSet blankPoints;
	
	// iterate over projection
	for(int x=xmin;x<=xmax;x++)
	{
		for(int y=ymin;y<=ymax;y++)
		{
			float xi,yi,zi;

			printf("%d,%d : %f\n",x,y,projection[x][y][0]);
			
			if (projection[x][y][0]==PROJECTION_BLANK)
			{
			  if (InterpolatePoint(x,y,xmin,xmax,ymin,ymax,xi,yi,zi))
			  {				  
				  blankPoints.push_back(std::tuple<float,float,float>(xi,yi,zi));
			  }
			}
		}
	}	
	
	return blankPoints;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: add_hole_particles_flattened <points> <output>\n");
		return -1;
	}
	
	int xmin,xmax,ymin,ymax;
	
	InitializePrjection();
	
	printf("Loading flattened points...\n");

	pointSet volume = LoadVolume(argv[1]);
	
	printf("Loaded %d flattened points\n",(int)volume.size());

    ProjectVolume(volume,xmin,xmax,ymin,ymax);	

	printf("Projected\n");
	
	pointSet blankPoints = Interpolate(xmin,xmax,ymin,ymax);
	
	FILE *f = fopen(argv[2],"w");
	
	if (f)
	{
		for(const std::tuple<float,float,float> &p : blankPoints)
		{
			// Only output hole particle if it is > 1 from existing particle
			bool tooClose = false;
			
			for(const std::tuple<float,float,float> &q : volume)
			{
				float dx = std::get<0>(p)-std::get<0>(q);
				float dy = std::get<1>(p)-std::get<1>(q);
				float dz = std::get<2>(p)-std::get<2>(q);
				
				if (dx*dx+dy*dy+dz*dz<1.0f)
				{
					tooClose = true;
					printf("Too close: %f,%f,%f and %f,%f,%f\n",std::get<0>(p),std::get<1>(p),std::get<2>(p),std::get<0>(q),std::get<1>(q),std::get<2>(q));
				}
			}
			
			if (!tooClose)
			  fprintf(f,"%f,%f,%f\n",std::get<0>(p),std::get<2>(p),std::get<1>(p));
		}
		
		fclose(f);
	}
}




