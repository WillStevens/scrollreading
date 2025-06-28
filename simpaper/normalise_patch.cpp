#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <tuple>

typedef std::tuple<float,float,float,float,float> paperPoint;
std::vector<paperPoint> paperPoints;

int main(int argc,char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr,"Usage: normalise_patch <patch>\n");
		fprintf(stderr,"Normalise patch so that min x,y is 0,0. On stderr output the max extent.\n");
		exit(-1);
	}
	
    float xmin,ymin,xmax,ymax;
	
	FILE *fi = fopen(argv[1],"r");

	if(fi)
	{
		bool first = true;
		
		float x,y,xp,yp,zp;
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
			if (x<xmin || first) xmin=x;
			if (y<ymin || first) ymin=y;
			if (x>xmax || first) xmax=x;
			if (y>ymax || first) ymax=y;
			
			first = false;

			paperPoints.push_back(paperPoint(x,y,xp,yp,zp));

		}
		
		fclose(fi);
	}

	
	for(const paperPoint &p : paperPoints)
	{
		printf("%f,%f,%f,%f,%f\n",std::get<0>(p)-xmin,std::get<1>(p)-ymin,std::get<2>(p),std::get<3>(p),std::get<4>(p));
	}
	
	fprintf(stderr,"(xmax-xmin,ymax-ymin)=(%f,%f)\n",xmax-xmin,ymax-ymin);
	
	return 0;
}
