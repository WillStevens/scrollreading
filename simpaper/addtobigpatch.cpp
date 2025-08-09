#include "bigpatch.cpp"

std::vector<gridPoint> gridPoints;

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

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
	if (argc != 4)
	{
		printf("Usage: addtobigpatch <patch 1> <patch 2> <patchno>\n");
		printf("Patch 1 is a bigpatch, patch 2 is a BIN patch\n");
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