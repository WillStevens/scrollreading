// Find the volume coords of the centre of a patch
#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: patch_centre <patch.bin>\n");
		printf("Outputs the volume coords (vx,vy,vz) of the centre of a patch\n");
		exit(-1);
	}
	
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
			
			if (x==0 && y==0)
			{
				printf("%f,%f,%f\n",px,py,pz);
				break;
			}
		}
		
		fclose(f);
	}
	
	exit(0);
}