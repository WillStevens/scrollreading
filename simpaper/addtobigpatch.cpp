#include "bigpatch.cpp"

std::vector<gridPoint> gridPoints;

void LoadPointSet(const char *fname)
{
	FILE *f = fopen(fname,"r");

	int l = 0;
		
	float x,y;
	float xp,yp,zp;
	
	if (f)
	{
	    while (fscanf(f,"%f,%f,%f,%f,%f\n",&x,&y,&xp,&yp,&zp)==5)
	    {
		  l++;
		  
		  gridPoints.push_back(gridPoint(x,y,xp,yp,zp));
	    }
	  
	  fclose(f);
	  
	  //printf("Loaded %d points\n",l);
	}
	else
	{
		//printf("Unable to open file %s\n",fname);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: addtobigpatch <patch 1> <patch 2>\n");
		printf("Patch 1 is a bigpatch, patch 2 is a CSV patch\n");
		exit(-1);
	}

  BigPatch *bp = OpenBigPatch(argv[1]);
  
  LoadPointSet(argv[2]);

  std::map<chunkIndex,std::vector<gridPoint>> chunkedPoints;
  
  for(const gridPoint &gp : gridPoints)
  {
	  chunkIndex ci = GetChunkIndex(std::get<0>(gp),std::get<1>(gp),std::get<2>(gp));
	  
	  if (chunkedPoints.count(ci)==0)
	  {
		  chunkedPoints[ci] = std::vector<gridPoint>();
	  }
	  
	  chunkedPoints[ci].push_back(gp);
  }
  
  for(auto const &item : chunkedPoints)
  {  
    WritePatchPoints(bp,item.first,item.second);
  }
  
  CloseBigPatch(bp);
 
  return 0; 
}