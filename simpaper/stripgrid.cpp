// Given 2 patches and an affine transformation, output a merged patch file
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr,"Usage: stripgrid <patch>\n");
		fprintf(stderr,"Removes the first two numbers from each line\n");
		exit(-1);
	}
	
	FILE *fi = fopen(argv[1],"r");
	
	if(fi)
	{		
		float x,y,xp,yp,zp;
		
		while(fscanf(fi,"%f,%f,%f,%f,%f",&x,&y,&xp,&yp,&zp)==5)
		{
		  printf("%f,%f,%f\n",xp,yp,zp);
		}
		
		fclose(fi);
	}
	
	exit(0);
}
