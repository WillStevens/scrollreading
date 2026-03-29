#include <math.h>

#include <map>
#include <vector>
#include <set>

#include "bigpatch.h"

#define CELL_SIZE 10
typedef std::tuple<int,int,int> gridCell;

std::vector<gridPoint> gridPoints[2];

std::map< gridCell, std::vector<gridPoint> > cellMap0;
std::map< gridCell, std::vector<gridPoint> > cellMap1;

typedef std::map< gridCell, std::vector<gridPoint> >::iterator cellMapIterator;

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



int ErasePoints(BigPatch *bp0, Patch &p1, int which, float radius)
{
  // Either erase bp0 points that are in p1, or erase p1 points that are in bp0. 'which' indicates which patch points should be erased from (0 or 1). 'radius' is the radius used for erasure.
	    
  gridPoints[0].clear();
  gridPoints[1].clear();
  cellMap0.clear();
  cellMap1.clear();
  
  std::set<chunkIndex> chunks;
	
  for(const patchPoint &pp : p1.points)
  {
	gridPoints[1].push_back(gridPoint(pp.x,pp.y,pp.v.x,pp.v.y,pp.v.z,1));
	// Which chunk is the point in?
	chunks.insert(GetChunkIndex(pp.v.x,pp.v.y,pp.v.z));
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
	ReadPatchPoints(bp0,chunk,gridPoints[0]);
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
      // TODO - replace all points in the patch...
	  p1.points.clear();
	  for(auto &gp : gridPointsOutput)
	  {
		  p1.points.push_back(patchPoint(std::get<0>(gp),std::get<1>(gp),std::get<2>(gp),std::get<3>(gp),std::get<4>(gp)));
	  }
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
				    WritePatchPoints(bp0,lastCi,batch);
			  }
			  
			  EraseChunk(bp0,ci);
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
	    WritePatchPoints(bp0,lastCi,batch);
  }
  
  return 0;
}

int ErasePoints(BigPatch *bp0, float x, float y, float z, int which, float radius)
{
	Patch p;
	
	p.points.push_back(patchPoint(0,0,x,y,z));
	
	return ErasePoints(bp0,p,which,radius);
}
