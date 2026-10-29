// Find the volume coords of the centre of a patch
#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: csv2bin <patch.csv> <patch.bin>\n");
		printf("Outputs bin representation of a csv patch\n");
		exit(-1);
	}
	
	FILE *fin = fopen(argv[1],"r");
	
	FILE *fout = fopen(argv[2],"wb");
	gridPointStruct p;

	if (fin && fout)
	{
		while(fscanf(fin,"%f,%f,%f,%f,%f",&p.x,&p.y,&p.px,&p.py,&p.pz)==5)
		{
			fwrite(&p,sizeof(p),1,fout);
		}
		
		fclose(fout);
		fclose(fin);
	}
	else
	{
		if (!fin)
			printf("Couldn't open input file\n");
		if (!fout)
			printf("Couldn't open output file\n");
	}
	
	exit(0);
}