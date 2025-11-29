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
	if (argc != 5 && argc != 2)
	{
		printf("Usage: listbigpatchpoints <patch> <cz> <cy> <cx>\n");
		printf("List points in a big patch");
		exit(-1);
	}

  BigPatch *bp = OpenBigPatch(argv[1]);

  if (bp)
  {
    std::vector<chunkIndex> chunkIndices;

	if (argc==2)
	  chunkIndices = GetAllPatchChunks(bp);
    else
	{
	  
      int cz = atoi(argv[2]);
      int cy = atoi(argv[3]);
      int cx = atoi(argv[4]);
      chunkIndices.push_back(chunkIndex(cx,cy,cz));
	}
	
	for(auto &ci : chunkIndices)
	{
      std::vector<gridPoint> gridPoints;
      ReadPatchPoints(bp, ci,gridPoints);
      for(auto &gp : gridPoints)	
        printf("%f %f %f %f %f %d\n",std::get<0>(gp),std::get<1>(gp),std::get<2>(gp),std::get<3>(gp),std::get<4>(gp),std::get<5>(gp));
    }
	
    CloseBigPatch(bp);

    return 0;
  }
  else
  {
    return -1;
  }	
}