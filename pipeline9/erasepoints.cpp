#include <math.h>

#include <map>
#include <vector>
#include <set>

#include "bigpatch.h"
#include "cellgrid.h"

#define CELL_SIZE 10
typedef std::tuple<int,int,int> gridCell;

std::vector<gridPoint> gridPoints[2];

// the cells of gridPoints[0] and gridPoints[1]; a flat hash in place of std::map< gridCell, std::vector<gridPoint> >
CellGrid<gridPoint> cellMap0;
CellGrid<gridPoint> cellMap1;

static void EraseCell(const gridPoint &gp, int &xc, int &yc, int &zc)
{
	xc = ((int)std::get<2>(gp))/CELL_SIZE;
	yc = ((int)std::get<3>(gp))/CELL_SIZE;
	zc = ((int)std::get<4>(gp))/CELL_SIZE;
}

void FillCellMap(void)
{
	cellMap0.Build(gridPoints[0],EraseCell);
	cellMap1.Build(gridPoints[1],EraseCell);
}

void FindMatches(std::set<gridPoint> &matchSet,int which, float radius)
{
	// The cells of cellMap0 in the order std::map iterated them (x, then y, then z); matchSet is a
	// std::set, but the order is kept anyway so that nothing here depends on that argument.
	std::vector<int> order;
	cellMap0.SortedCells(order);
	for(int c0 : order)
	{
		int g0x = cellMap0.X(c0);
		int g0y = cellMap0.Y(c0);
		int g0z = cellMap0.Z(c0);

		// The occupied neighbour cells of cellMap1, found once, in the nx, ny, nz order of the loops below
		int neighbours[27];
		int numNeighbours = 0;
		for(int nx = g0x-1; nx<=g0x+1; nx++)
		for(int ny = g0y-1; ny<=g0y+1; ny++)
		for(int nz = g0z-1; nz<=g0z+1; nz++)
		{
			int c1 = cellMap1.Find(nx,ny,nz);
			if (c1 >= 0)
				neighbours[numNeighbours++] = c1;
		}
		bool foundAny = numNeighbours > 0;
				
		if (foundAny)
		{
		    for(const gridPoint &gp0 : cellMap0.Cell(c0))
			{
				float x0 = std::get<0>(gp0);
				float y0 = std::get<1>(gp0);
				float xp0 = std::get<2>(gp0);
				float yp0 = std::get<3>(gp0);
				float zp0 = std::get<4>(gp0);
				int p0 = std::get<5>(gp0);

					
				for(int k = 0; k < numNeighbours; k++)
				{
					{
						for(const gridPoint &gp1 : cellMap1.Cell(neighbours[k]))
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
  
  std::set<chunkIndex> chunks;
	
  for(PatchIterator pi = p1.Begin(); p1.Next(pi);)
  {
	gridPoints[1].push_back(gridPoint(pi.p->x,pi.p->y,pi.p->v.x,pi.p->v.y,pi.p->v.z,1));
	// Which chunk is the point in?
	chunks.insert(GetChunkIndex(pi.p->v.x,pi.p->v.y,pi.p->v.z));
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
	  std::vector<patchPoint> points;
	  p1.Clear();
	  for(auto &gp : gridPointsOutput)
	  {
		  points.push_back(patchPoint(std::get<0>(gp),std::get<1>(gp),std::get<2>(gp),std::get<3>(gp),std::get<4>(gp)));
	  }
	  
	  // There might be 0 points, make sure Patch can handle this
	  p1.BuildFromPoints(points,p1.patchNum);
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
				  //printf("Writing %d points to %d.%d.%d\n",(int)batch.size(),std::get<2>(lastCi),std::get<1>(lastCi),std::get<0>(lastCi));
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
	  //printf("Writing %d points to %d.%d.%d\n",(int)batch.size(),std::get<2>(lastCi),std::get<1>(lastCi),std::get<0>(lastCi));
	  if (batch.size() != 0)
	    WritePatchPoints(bp0,lastCi,batch);
  }
  
  return 0;
}

int ErasePoints(BigPatch *bp0, float x, float y, float z, int which, float radius)
{
	Patch p;
	std::vector<patchPoint> pts;
	pts.push_back(patchPoint(0,0,x,y,z));
	
	p.BuildFromPoints(pts,0);
	
	return ErasePoints(bp0,p,which,radius);
}
