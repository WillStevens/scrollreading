// Find the volume coords of the centre of a patch
#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: bin2csv <patch.bin>\n");
		printf("Outputs csv representation of a patch on standard output\n");
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
			
			printf("%f,%f,%f,%f,%f\n",p.x,p.y,p.px,p.py,p.pz);
		}
		
		fclose(f);
	}
	
	exit(0);
}