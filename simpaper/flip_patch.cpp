#include <stdio.h>
#include <stdlib.h>

int main(int argc,char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr,"Usage: flip_patch <patch>\n");
		fprintf(stderr,"Flip a patch, output on stdout.\n");
		exit(-1);
	}
	
    float xmin,ymin,xmax,ymax;
	
	FILE *fi = fopen(argv[1],"r");

	if(fi)
	{
		bool first = true;
		
		float x,y,xd,yd,xp,yp,zp;
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
			if (x<xmin || first) xmin=x;
			if (y<ymin || first) ymin=y;
			if (x>xmax || first) xmax=x;
			if (y>ymax || first) ymax=y;
			
			first = false;
		}
		
		fclose(fi);
	}

	fi = fopen(argv[1],"r");
	
	if(fi)
	{
		float x,y,xd,yd,xp,yp,zp;
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
			xd = xmax-x;
			yd = y-ymin;
			
			printf("%f,%f,%f,%f,%f\n",xd,yd,xp,yp,zp);
		}
		
		fclose(fi);
	}
		
	return 0;
}
