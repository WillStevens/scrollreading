#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <tuple>

typedef std::tuple<float,float,float> point;
typedef std::vector< point > pointVector;

int main(int argc,char *argv[])
{
	if (argc != 5)
	{
		fprintf(stderr,"Usage: find_nearest <patch> x y r\n");
		fprintf(stderr,"Find all of the grid points within a radius r of x,y, write them to stdout.\n");
		exit(-1);
	}
	
	float xTarg = atof(argv[2]);
	float yTarg = atof(argv[3]);
	float radius2 = atof(argv[4]);
    radius2=radius2*radius2; // square of the radius
	
    float xmin=0,ymin=0,zmin=0,minR2=0;

	pointVector pv;
	
	FILE *fi = fopen(argv[1],"r");

	if(fi)
	{
		
		bool first = true;
		
		float x,y,xp,yp,zp,r2;
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
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
		
		fclose(fi);
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
