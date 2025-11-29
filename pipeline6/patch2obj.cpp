#include <stdio.h>

#include "parameters.h"

#define SHEET_SIZE 1000

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPoint;

bool ConvertPatch(char *filename, char *output)
{  
  gridPoint p;
  
  FILE *f = fopen(filename,"r");
  FILE *fo = fopen(output,"w");
  
  if (f && fo)
  {
	fseek(f,0,SEEK_END);
	long fsize = ftell(f);
	fseek(f,0,SEEK_SET);
  
    // input in x,y,z order 
    while(ftell(f)<fsize)
    {
	  fread(&p,sizeof(p),1,f);
	
	  fprintf(fo,"v %f %f %f\n",p.px,p.py,p.pz);	  
    }
	
	if (fo) fclose(fo);
    if (f) fclose(f);
	
	return true;
  }

  if (fo) fclose(fo);
  if (f) fclose(f);
  
  return false;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: patch2obj <input> <output>\n");
		printf("Converts a patch to an obj file\n");
	}
	
	if (ConvertPatch(argv[1],argv[2]))
	{
		printf("Converted\n");
		
		return 0;
	}
	else
	{
		printf("Failed to convert\n");
		
		return -1;
	}
}
