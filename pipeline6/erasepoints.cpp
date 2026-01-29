#include <math.h>

#include <map>
#include <vector>
#include <set>

#include "bigpatch.cpp"


typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

#define CELL_SIZE 10
typedef std::tuple<int,int,int> gridCell;

std::vector<gridPoint> gridPoints[2];

std::map< gridCell, std::vector<gridPoint> > cellMap0;
std::map< gridCell, std::vector<gridPoint> > cellMap1;

typedef std::map< gridCell, std::vector<gridPoint> >::iterator cellMapIterator;

float Distance(float x0, float y0, float z0, float x1, float y1, float z1)
{
	float dx = x1-x0, dy = y1-y0, dz = z1-z0;
	
	float d2 = dx*dx+dy*dy+dz*dz;
	
	return sqrt(d2);
}

void LoadPointSet(const char *fname,int index)
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
			gridPoints[index].push_back(gridPoint(x,y,px,py,pz,0));
		}
	  
	    fclose(f);
	}
	else
	{
		printf("Unable to open file %s\n",fname);
	}
}

void SavePointSet(const char *fname, std::vector<gridPoint>& gps)
{
	FILE *f = fopen(fname,"w");
	
	if (f)
	{
		gridPointStruct p;
  
		// input in x,y,z order 
		for(auto &gp : gps)
		{
			p.x = std::get<0>(gp);
			p.y = std::get<1>(gp);
			p.px = std::get<2>(gp);
			p.py = std::get<3>(gp);
			p.pz = std::get<4>(gp);
			
			fwrite(&p,sizeof(p),1,f);			
		}
	  
	    fclose(f);
	}
	else
	{
		printf("Unable to open file %s\n",fname);
	}
}


void FillCellMap(void)
{
	for(auto const &gp : gridPoints[0])
	{
	  int xc = ((int)std::get<2>(gp))/CELL_SIZE;
	  int yc = ((int)std::get<3>(gp))/CELL_SIZE;
	  int zc = ((int)std::get<4>(gp))/CELL_SIZE;

      gridCell g(xc,yc,zc);

      if (cellMap0.count(g)==0)
        cellMap0[g] = std::vector<gridPoint>();			  
      cellMap0[g].push_back(gp);
	}

	for(auto const &gp : gridPoints[1])
	{
	  int xc = ((int)std::get<2>(gp))/CELL_SIZE;
	  int yc = ((int)std::get<3>(gp))/CELL_SIZE;
	  int zc = ((int)std::get<4>(gp))/CELL_SIZE;

      gridCell g(xc,yc,zc);

      if (cellMap1.count(g)==0)
        cellMap1[g] = std::vector<gridPoint>();			  
      cellMap1[g].push_back(gp);
	}
}

void FindMatches(std::set<gridPoint> &matchSet,int which, float radius)
{
	for(cellMapIterator i = cellMap0.begin(); i!= cellMap0.end(); i++)
	{
		int g0x = std::get<0>(i->first);
		int g0y = std::get<1>(i->first);
		int g0z = std::get<2>(i->first);
				
		bool foundAny = false;
		for(int nx = g0x-1; nx<=g0x+1; nx++)
		for(int ny = g0y-1; ny<=g0y+1; ny++)
		for(int nz = g0z-1; nz<=g0z+1; nz++)
		{
			gridCell g(nx,ny,nz);
			if (cellMap1.count(g) != 0)
			{
				// There is an overlapping cell, so add to the list of patches that could be involved
				foundAny = true;
			}
		}
		
				
		if (foundAny)
		{
		    for(const gridPoint &gp0 : cellMap0[i->first])
			{
				float x0 = std::get<0>(gp0);
				float y0 = std::get<1>(gp0);
				float xp0 = std::get<2>(gp0);
				float yp0 = std::get<3>(gp0);
				float zp0 = std::get<4>(gp0);
				int p0 = std::get<5>(gp0);

					
				for(int nx = g0x-1; nx<=g0x+1; nx++)
				for(int ny = g0y-1; ny<=g0y+1; ny++)
				for(int nz = g0z-1; nz<=g0z+1; nz++)
				{
					gridCell g(nx,ny,nz);
					if (cellMap1.count(g) != 0)
					{		
						for(const gridPoint &gp1 : cellMap1[g])
						{								
							float x1 = std::get<0>(gp1);
							float y1 = std::get<1>(gp1);
							float xp1 = std::get<2>(gp1);
							float yp1 = std::get<3>(gp1);
							float zp1 = std::get<4>(gp1);
				            int p1 = std::get<5>(gp1);
			  
							float d = Distance(xp0,yp0,zp0,xp1,yp1,zp1);
							if (d<radius)
							{
								if (which==0)
								{
									matchSet.insert(gridPoint(x0,y0,xp0,yp0,zp0,p0));
								}
								else
								{
									matchSet.insert(gridPoint(x1,y1,xp1,yp1,zp1,p1));
								}
							}
						}
					}
				}					
			}
		}
	}
}



int main(int argc, char *argv[])
{
  if (argc != 5 && argc != 7)
  {
	printf("Usage: erasepoints <patch0> <patch1> <which> <radius>\n");
	printf("Usage: erasepoints <patch0> x y z 0 <radius>  (where patch0 must be a bigpatch\n");
	printf("patch0 is a bigpatch, patch1 is a binary small patch. Either erase patch0 points that are in patch1, or erase patch1 points that are in patch0. 'which' indicates which patch points should be erased from (0 or 1). 'radius' is the radius used for erasure.\n");
	exit(-1);
  }

  bool single = argc==7;

  int which = atoi(argv[single?5:3]);
  float radius = atof(argv[single?6:4]);
  float px,py,pz;

  
  if (single && which != 0)
  {
	printf("If specifying a single point x,y,z then which must be 0\n");
	exit(-2);
  }

  if (single)
  {
    px = atof(argv[2]);
    py = atof(argv[3]);
    pz = atof(argv[4]);
	
	gridPoints[1].push_back(gridPoint(0,0,px,py,pz,0));
  }
  else
  {
	LoadPointSet(argv[2],1);
  }
  BigPatch *bp = OpenBigPatch(argv[1]);
	  
  if (!bp)
  {
	printf("Failed to open bigpatch:%s\n",argv[1]);
	exit(-2);
  }
	    
  std::set<chunkIndex> chunks;
	
  for(const gridPoint &gp : gridPoints[1])
  {
	// Which chunk is the point in?
	chunks.insert(GetChunkIndex(std::get<2>(gp),std::get<3>(gp),std::get<4>(gp)));
  }
  
  std::set<chunkIndex> expanded;
  for(const chunkIndex &chunk : chunks)
  {
	for(int xo=-1; xo<=1; xo++)
	for(int yo=-1; yo<=1; yo++)
	for(int zo=-1; zo<=1; zo++)
  	{
	  expanded.insert(chunkIndex(std::get<0>(chunk)+xo,std::get<1>(chunk)+yo,std::get<2>(chunk)+zo));
	}
  }
	
  chunks.insert(expanded.begin(),expanded.end());
  
  for(const chunkIndex &chunk : chunks)
  {
	ReadPatchPoints(bp,chunk,gridPoints[0]);
  }
		
  FillCellMap();

  std::set<gridPoint> matchSet;
  
  FindMatches(matchSet,which,radius);

  printf("erasepoints found %d matches\n",(int)matchSet.size());
  
  std::vector<gridPoint> gridPointsOutput;
  std::vector<bool> outputFlag;
  
  for(auto &gp : gridPoints[which])
  {
	  outputFlag.push_back(matchSet.count(gp)==0);
	  if (matchSet.count(gp)==0)
	  {
		  gridPointsOutput.push_back(gp);
	  }
  }

  printf("Input points:%d\n",(int)gridPoints[which].size());
  printf("Output points:%d\n",(int)gridPointsOutput.size());
  
  if (which==1)
  {	  
	  SavePointSet(argv[2],gridPointsOutput);
  }
  else
  {
	  chunkIndex lastCi;
	  std::vector<gridPoint> batch;
	  
	  int i = 0;
	  for(auto &gp : gridPoints[which])
	  {
		  float xp = std::get<2>(gp);
		  float yp = std::get<3>(gp);
	      float zp = std::get<4>(gp);
		  
		  chunkIndex ci = GetChunkIndex(xp,yp,zp);
		  
		  if (i==0 || ci != lastCi)
		  {
			  if (i)
			  {
				  //printf("Writing %d points to %s\n",(int)batch.size(),argv[1]);
	              if (batch.size() != 0)
				    WritePatchPoints(bp,lastCi,batch);
			  }
			  
			  EraseChunk(bp,ci);
			  batch.clear();
		  }
		  lastCi = ci;
		  
		  if (outputFlag[i])
		    batch.push_back(gp);
		  
		  i++;
	  }  

	  // Last batch isn't written within the loop
	  //printf("Writing %d points to %s\n",(int)batch.size(),argv[1]);
	  if (batch.size() != 0)
	    WritePatchPoints(bp,lastCi,batch);
	  
      CloseBigPatch(bp);
  }
  
  return 0;
}