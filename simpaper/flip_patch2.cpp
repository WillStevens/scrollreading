#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int main(int argc,char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr,"Usage: flip_patch <patch in> <patch out>\n");
		fprintf(stderr,"Flip a BIN patch.\n");
		exit(-1);
	}
	
	
	FILE *fi = fopen(argv[1],"r");
	FILE *fo = fopen(argv[2],"w");

	if(fi && fo)
	{
		bool first = true;

		gridPointStruct p;
		fseek(fi,0,SEEK_END);
		long fsize = ftell(fi);
		fseek(fi,0,SEEK_SET);
  
		while(ftell(fi)<fsize)
		{
			fread(&p,sizeof(p),1,fi);
			p.x = -p.x;
			p.y = p.y;
	
            fwrite(&p,sizeof(p),1,fo);	
		}
		
		fclose(fi);
		fclose(fo);
	}
	else
	{
		if (!fi) printf("Unable to open input file:%s\n",argv[1]);
		if (!fo) printf("Unable to open input file:%s\n",argv[2]);
	}
		
	return 0;
}
