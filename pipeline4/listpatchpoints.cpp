#include "bigpatch.cpp"

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

std::vector<gridPoint> gridPoints;

void LoadPointSet(const char *fname,int patch)
{
	FILE *f = fopen(fname,"r");
	
	if (f)
	{
		gridPointStruct p;
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
  
		// input in x,y,z order 
		while(ftell(f)<fsize)
		{
			fread(&p,sizeof(p),1,f);
			float x,y,px,py,pz;
			x=p.x;y=p.y;px=p.px;py=p.py;pz=p.pz;
			gridPoints.push_back(gridPoint(x,y,px,py,pz,patch));
		}
	  
	    fclose(f);
	}
	else
	{
		printf("Unable to open file %s\n",fname);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: listpatchpoints <patch>\n");
		printf("List points in a patch");
		exit(-1);
	}

  LoadPointSet(argv[1],0);

  for(auto &gp : gridPoints)	
    printf("%f %f %f %f %f %d\n",std::get<0>(gp),std::get<1>(gp),std::get<2>(gp),std::get<3>(gp),std::get<4>(gp),std::get<5>(gp));
	

  return 0;
}