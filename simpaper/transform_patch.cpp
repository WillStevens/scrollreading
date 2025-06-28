// Given 2 patches and an affine transformation, output a merged patch file
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char *argv[])
{
	if (argc != 8)
	{
		printf("Usage: transform_patch <patch> a b c d e f\n");
		printf("Apply affine transformation abcdef to patch and output the result\n");
	}
	
	float a = atof(argv[2]);
	float b = atof(argv[3]);
	float c = atof(argv[4]);
	float d = atof(argv[5]);
	float e = atof(argv[6]);
	float f = atof(argv[7]);
	
	FILE *fi = fopen(argv[1],"r");
	
	if(fi)
	{
		float x,y,xd,yd,xp,yp,zp;
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
			xd = a*x+b*y+c;
			yd = d*x+e*y+f;
			
			printf("%f,%f,%f,%f,%f\n",xd,yd,xp,yp,zp);
		}
		
		fclose(fi);
	}
}
