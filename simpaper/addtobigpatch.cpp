#include "bigpatch.cpp"

std::vector<gridPoint> gridPoints;

void LoadPointSet(const char *fname,int patch)
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
		  
		  gridPoints.push_back(gridPoint(x,y,xp,yp,zp,patch));
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
	if (argc != 4)
	{
		printf("Usage: addtobigpatch <patch 1> <patch 2> <patchno>\n");
		printf("Patch 1 is a bigpatch, patch 2 is a CSV patch\n");
		exit(-1);
	}

  BigPatch *bp = OpenBigPatch(argv[1]);
  
  LoadPointSet(argv[2],atoi(argv[3]));

  std::map<indexChunkIndex,std::vector<chunkIndex>> indexedChunks;
  std::map<chunkIndex,std::vector<gridPoint>> chunkedPoints;
  
  for(const gridPoint &gp : gridPoints)
  {
	  indexChunkIndex ici = GetIndexChunkIndex(std::get<0>(gp),std::get<1>(gp));
	  chunkIndex ci = GetChunkIndex(std::get<2>(gp),std::get<3>(gp),std::get<4>(gp));
	  
	  if (chunkedPoints.count(ci)==0)
	  {
		  chunkedPoints[ci] = std::vector<gridPoint>();
	  }
	  
	  chunkedPoints[ci].push_back(gp);
	  
	  if (indexedChunks.count(ici)==0)
	  {
		  indexedChunks[ici]=std::vector<chunkIndex>();
	  }
	  
	  indexedChunks[ici].push_back(ci);
  }
  
  for(auto const &item : chunkedPoints)
  {  
    WritePatchPoints(bp,item.first,item.second);
  }

  for(auto const &item : indexedChunks)
  {
	std::set<chunkIndex> chunkIndices;
    ReadIndexedChunks(bp,item.first,chunkIndices);
    chunkIndices.insert(item.second.begin(),item.second.end());	
    WriteIndexedChunks(bp,item.first,chunkIndices);
  }
  
  CloseBigPatch(bp);
 
  return 0; 
}