// Find the extent of a patch
#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: patch_extent <patch.bin>\n");
		printf("Outputs the volume extent (vx-min,vy-min,vz-min,vx-max,vy-max,vz-max) of a patch\n");
		exit(-1);
	}
	
	float xmin,ymin,zmin,xmax,ymax,zmax;
	bool first = true;
	
	FILE *f = fopen(argv[1],"r");
	gridPointStruct p;

	if (f)
	{
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
  
		// input in x,y,z order 
		while(ftell(f)<fsize)
		{
			fread(&p,sizeof(p),1,f);
			float x,y,px,py,pz;
			x=p.x;y=p.y;px=p.px;py=p.py;pz=p.pz;
	
			if (first || px<xmin) xmin=px;
			if (first || py<ymin) ymin=py;
			if (first || pz<zmin) zmin=pz;
			if (first || px>xmax) xmax=px;
			if (first || py>ymax) ymax=py;
			if (first || pz>zmax) zmax=pz;
			
			first = false;			
		}
		
		fclose(f);
	}
	
	printf("%f,%f,%f,%f,%f,%f\n",xmin,ymin,zmin,xmax,ymax,zmax);

	exit(0);
}