// Given 2 patches and an affine transformation, output a merged patch file
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: extent_patch <patch>\n");
		printf("Calculate cube enclosing patch\n");
	}
	
	FILE *fi = fopen(argv[1],"r");
	
	if(fi)
	{
		float xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
		
		float x,y,xp,yp,zp;
		bool first = true;
		
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
			if (first || xp<xmin) xmin=xp;
			if (first || yp<ymin) ymin=yp;
			if (first || zp<zmin) zmin=zp;
			if (first || xp>xmax) xmax=xp;
			if (first || yp>ymax) ymax=yp;
			if (first || zp>zmax) zmax=zp;
			
			first = false;
		}
		
		fclose(fi);

		printf("%f,%f,%f,%f,%f,%f\n",xmin,ymin,zmin,xmax,ymax,zmax);
	}
}
