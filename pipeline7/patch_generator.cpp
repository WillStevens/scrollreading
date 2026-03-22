#include <iostream>
#include <cmath>

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <utility>

#include <smmintrin.h>

using namespace std;

#include "parameters.h"

#include "patch_generator.h"

#define MARGIN 8 // Don't try to fill near the edges of the volume

#define EXPECTED_DISTANCE(xd,yd) (QUADMESH_SIZE*sqrt(xd*xd+yd*yd))

#define NEIGHBOUR_RADIUS 2
#define NEIGHBOUR_RADIUS_FLOAT 2.0

void PatchGenerator::MakeActive(int x, int y)
{
	active[x][y] = true;
	activeList[activeListSize][0] = x;
	activeList[activeListSize++][1] = y;
}


void PatchGenerator::ClearPointLookup(void)
{
	pointLookup.clear();
}

void PatchGenerator::AddPointToLookup(const point &p)
{
	pointInt q;
	q.x = ((int)p.x)/NEIGHBOUR_RADIUS;
	q.y = ((int)p.y)/NEIGHBOUR_RADIUS;
	q.z = ((int)p.z)/NEIGHBOUR_RADIUS;
	
	if (pointLookup.count(q)==0)
	  pointLookup[q] = pointSet();
	pointLookup[q].push_back(p);
}


float PatchGenerator::GetDistanceAtPoint(int xp, int yp, int zp)
{
	pointInt p;
	p.x = xp;
	p.y = yp;
	p.z = zp;
	
	if (distanceLookup.count(p) == 0)
	{
		distanceLookup[p] = ZARRRead_c64i1b256(vectorFieldZarr,zp,yp,xp,3);
	}
	return distanceLookup[p];
}

bool PatchGenerator::HasCloseNeighbour(const point &p)
{
	int xp = ((int)p.x)/NEIGHBOUR_RADIUS;
	int yp = ((int)p.y)/NEIGHBOUR_RADIUS;
	int zp = ((int)p.z)/NEIGHBOUR_RADIUS;
	
	pointInt q;
	for(q.x = xp-1; q.x<=xp+1; q.x++)
	for(q.y = yp-1; q.y<=yp+1; q.y++)
	for(q.z = zp-1; q.z<=zp+2; q.z++)
	{
		for(const point &r : pointLookup[q])
		{
			if ((p-r).length()<=NEIGHBOUR_RADIUS_FLOAT)
			{
				return true;
			}
		}
	}

	return false;
}

void PatchGenerator::SetVectorField(int x, int y)
{
  uint64_t px = (uint64_t)paperSheet[x][y].pos.x;
  uint64_t py = (uint64_t)paperSheet[x][y].pos.y;
  uint64_t pz = (uint64_t)paperSheet[x][y].pos.z;
  uint64_t p = (px<<32)+(py<<16)+pz;
  
  auto it = vectorFieldLookup.find(p);  
  if (it == vectorFieldLookup.end())
  {
	Vec3 v; // defaults to 0,0,0

    if (px-VOL_OFFSET_X>=0 && px-VOL_OFFSET_X<VOL_SIZE_X && py-VOL_OFFSET_Y>=0 && py-VOL_OFFSET_Y<VOL_SIZE_Y && pz-VOL_OFFSET_Z>=0 && pz-VOL_OFFSET_Z<VOL_SIZE_Z)
    {	
		v.x = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,0)*VECTORFIELD_CONSTANT;
		v.y = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,1)*VECTORFIELD_CONSTANT;
		v.z = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,2)*VECTORFIELD_CONSTANT;
	}

	vectorFieldLookup[p] = paperSheet[x][y].vectorField = v;
  }
  else
  {
	paperSheet[x][y].vectorField = it->second;
  }
  
  //printf("Vector field for %d,%d was %f,%f,%f,%f\n",x,y,paperVectorField[x][y][0],paperVectorField[x][y][1],paperVectorField[x][y][2],paperVectorField[x][y][3]);
}


void PatchGenerator::InitExpectedDistanceLookup(void)
{
  for(int x = 0; x<3; x++)
  for(int y = 0; y<3; y++)
  {
    expectedDistanceLookup[x][y] = QUADMESH_SIZE*sqrt((x-1)*(x-1)+(y-1)*(y-1));
  }
}

#define FORCES_INNERLOOP(xd,yd) \
      if (active[x+xd][y+yd]) \
	  { \
		Vec3 direction = posxy-paperSheet[x+xd][y+yd].pos; \
        float actualDistance = direction.length(); \
        direction /= actualDistance; \
        \
        float force = expectedDistanceLookup[xd+1][yd+1]-actualDistance; \
        \
        acc += direction*force; \
	  }

float PatchGenerator::ForcesAndMove(void)
{  
  float largestForce = 0.0;
    
  for(int ai = 0; ai<activeListSize; ai++)
  {
	int x = activeList[ai][0], y = activeList[ai][1];
	
    Vec3 acc = paperSheet[x][y].vectorField; // This will get multiplie by springForceConstant later
	                                          // So we have already divided by that in the constant definition for
											  // VECTORFIELD_CONSTANT
	Vec3 posxy = paperSheet[x][y].pos;

	//printf("Pos: %f %f %f\n",posxy.x,posxy.y,posxy.z);
	
	// Don't bother with range checks - make sure elsewhere that growth stops before we get to the edges
    FORCES_INNERLOOP(-1,-1)
	FORCES_INNERLOOP(-1,0)
	FORCES_INNERLOOP(-1,1)
	FORCES_INNERLOOP(0,-1)
	FORCES_INNERLOOP(0,1)
	FORCES_INNERLOOP(1,-1)
	FORCES_INNERLOOP(1,0)
	FORCES_INNERLOOP(1,1)

    acc *= SPRING_FORCE_CONSTANT;

    float forceMag = acc.lengthSquared();
		
    if (forceMag > largestForce)
      largestForce = forceMag;
  
    paperSheet[x][y].vel *= FRICTION_CONSTANT;
	
    paperSheet[x][y].vel += acc;
  }

  for(int ai = 0; ai<activeListSize; ai++)
  {
	int x = activeList[ai][0], y = activeList[ai][1];

    point oldPos = paperSheet[x][y].pos;
    paperSheet[x][y].pos += paperSheet[x][y].vel;
		
	if (int(oldPos.x) != int(paperSheet[x][y].pos.x) || int(oldPos.y) != int(paperSheet[x][y].pos.y) || int(oldPos.z) != int(paperSheet[x][y].pos.z))
	  SetVectorField(x,y);
  }
  
  return sqrt(largestForce);
}

bool PatchGenerator::TryToFill(int xp, int yp, Vec3 &rp)
{
  if (!active[xp][yp])
  {
    for(int i = 0; i<4; i++)
	{
      int xd = dirVectorLookup[i][0];
	  int yd = dirVectorLookup[i][1];
	  
      int xo1 = xp+xd;
	  int xo2 = xp+xd+xd;
	  int yo1 = yp+yd;
	  int yo2 = yp+yd+yd;

	  // corner point (RH)
      int xc1 = xp+xd-yd;
	  int yc1 = yp+xd+yd;
      // corner point the other side of xo1,yo1	(LH)  
      int xd1 = xp+xd+yd;
	  int yd1 = yp+yd-xd;

	  // right angles to xo1,yo1 (going right)
      int xa1 = xp-yd;
	  int ya1 = yp+xd;
/*
      if (active[xo1][yo1])
	  {
		  printf("%d,%d has active neighbour %d %d\n",xp,yp,xo1,yo1);
		  printf("%d %d status = %s\n",xo2,yo2,active[xo2][yo2]?"active":"inactive");
		  printf("%d %d status = %s\n",xc1,yc1,active[xc1][yc1]?"active":"inactive");
		  printf("%d %d status = %s\n",xd1,yd1,active[xd1][yd1]?"active":"inactive");
		  printf("%d %d status = %s\n",xa1,ya1,active[xa1][ya1]?"active":"inactive");
	  }
*/	  
      // If xo2,yo2 is in bound, then no need to check xo1,yo1
	  if (xo2>=0 && xo2<SHEET_SIZE && yo2>=0 && yo2<SHEET_SIZE && active[xo1][yo1] && active[xo2][yo2] && xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && xd1>=0 && xd1<SHEET_SIZE && yd1>=0 && yd1<SHEET_SIZE && active[xc1][yc1] && active[xd1][yd1])
	  {
		// Adjacent point, point beyond that, and points either side of adjacent point are all active
		rp = 2*paperSheet[xo1][yo1].pos - paperSheet[xo2][yo2].pos;
		//printf("Straightfill %f %f %f\n",rp.x,rp.y,rp.z);
		return true;
	  }
      // If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if (xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && active[xo1][yo1] && active[xa1][ya1] && active[xc1][yc1])
	  {
          // Get midpoint of xo1,yo1,xa1,ya1
		  rp = (paperSheet[xo1][yo1].pos+paperSheet[xa1][ya1].pos)/2;
          // Extend from the corner through the midpoint
          rp = 2*rp - paperSheet[xc1][yc1].pos;
		  //printf("Cornerfill %f %f %f\n",rp.x,rp.y,rp.z);
		  return true;
	  }
	}
  }
  return false;
}

bool PatchGenerator::MarkHighStress(void)
{  
  bool r = false;
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
	{
	  float SETot = 0;
	  int SENum = 0;
	  // Don't bother with range check - make sure elsewhere that edges of sheet are not approached
      for(int xo=x-1; xo<x+2; xo++)
      for(int yo=y-1; yo<y+2; yo++)
        if (active[xo][yo] && ! (xo == x && yo == y)) 
		{
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            Vec3 direction = paperSheet[x][y].pos-paperSheet[xo][yo].pos;
            float actualDistance = direction.length();
                
            float force = (expectedDistance - actualDistance);
            SETot += force*force;
			SENum += 1;
		}
		
	  if (SETot/SENum > HIGH_STRESS_THRESHHOLD)
	  {
	    // paperPos is in x,y,z order
		int xb = (int)((paperSheet[x][y].pos.x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE);
		int yb = (int)((paperSheet[x][y].pos.y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE);
		int zb = (int)((paperSheet[x][y].pos.z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE);
		
		for(int xo = xb-(xb>0); xo <= xb+(xb+1<VOL_SIZE_X/STRESS_BLOCK_SIZE); xo++)
		for(int yo = yb-(yb>0); yo <= yb+(yb+1<VOL_SIZE_Y/STRESS_BLOCK_SIZE); yo++)
		for(int zo = zb-(zb>0); zo <= zb+(zb+1<VOL_SIZE_Z/STRESS_BLOCK_SIZE); zo++)
 		{
			highStress[zo][yo][xo] = true;
			r = true;
		}
		
		printf("\nHigh stress at x,y,z=%f,%f,%f",paperSheet[x][y].pos.x,paperSheet[x][y].pos.y,paperSheet[x][y].pos.z);
	  }
	}
	
	return r;
}

bool PatchGenerator::HasHighStress(int x, int y, int z)
{
	return highStress[(z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE][(y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE][(x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE];
}

// TODO newPtsPaper needs to be vector of int vec2
int PatchGenerator::MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper)
{
    Vec3 np;

	ClearPointLookup();
    for(int x = 0; x<SHEET_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE; y++)
    {
		if (active[x][y])
			AddPointToLookup(point(paperSheet[x][y].pos));
	}
	std::set<pair<int,int>> inactiveNeighbours;
    for(int ai = 0; ai<activeListSize; ai++)
    {
      int xa = activeList[ai][0], ya = activeList[ai][1];
	  int offsets[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
	  for(int oi = 0; oi<4; oi++)
	  {
		  int x = xa+offsets[oi][0];
		  int y = ya+offsets[oi][1];
		  
		  if (!active[x][y]) inactiveNeighbours.insert(pair<int,int>(x,y));
      }
	}
	for(auto &p : inactiveNeighbours)
	{
		int x = p.first, y = p.second;
		  if (TryToFill(x,y,np))
		  { 
			float dist;
			bool okayToFill = true;
			int px = (int)np.x;
			int py = (int)np.y;
			int pz = (int)np.z;
			if (px-VOL_OFFSET_X>=MARGIN && px-VOL_OFFSET_X<VOL_SIZE_X-MARGIN && py-VOL_OFFSET_Y>=MARGIN && py-VOL_OFFSET_Y<VOL_SIZE_Y-MARGIN && pz-VOL_OFFSET_Z>=MARGIN && pz-VOL_OFFSET_Z<VOL_SIZE_Z-MARGIN)
			{
			  if (HasHighStress(px,py,pz))
			  {
				  //printf("1\n");
				  okayToFill = false;
			  }
			  else if ((dist=GetDistanceAtPoint(px,py,pz))>0)
			  {
				  //printf("2:%f\n",dist);
				  okayToFill = false;
			  }
			  else if (HasCloseNeighbour(np))
			  {
				  //printf("3\n");
				  okayToFill = false;
			  }
			  
			  if (okayToFill)
			  {
				//printf("New point at %d,%d\n",x,y);  
				newPtsPaper.push_back(point(x,y,0.0));
				newPts.push_back(np);
			  }
			}
			else
			{
				//printf("out of bounds at %d,%d,%d\n",px,py,pz);  
			}
		  }
      
    }
    //printf("%d points added\n",(int)newPts.size());     

    return newPts.size();	
}

void PatchGenerator::AddNewPoints(pointSet &newPts, pointSet &newPtsPaper)
{
    for(int i = 0; i<(int)newPts.size(); i++)
    {
	  point paperPoint = newPtsPaper[i];
	  int x = paperPoint.x;
	  int y = paperPoint.y;
	  paperSheet[x][y].pos = newPts[i];

	  paperSheet[x][y].vel.x = 0.0;
	  paperSheet[x][y].vel.y = 0.0;
	  paperSheet[x][y].vel.z = 0.0;
	  
      MakeActive(x,y);
	  SetVectorField(x,y);
    }
}

PatchGenerator::PatchGenerator(const string &surfaceZarrName,const string &vectorFieldZarrName)
{
	printf("Opening zarrs\n");
	surfaceZarr = ZARROpen_1(surfaceZarrName.c_str());
    vectorFieldZarr = ZARROpen_c64i1b256(vectorFieldZarrName.c_str());

	printf("Init\n");

    InitExpectedDistanceLookup();

    activeListSize = 0;	
}
		
PatchGenerator::~PatchGenerator(void)
{
	ZARRClose_c64i1b256(vectorFieldZarr);
	ZARRClose_1(surfaceZarr);
}

bool PatchGenerator::SetSeed(float seed[9])
{  
    if (ZARRRead_1(surfaceZarr,seed[2],seed[1],seed[0]) != 255)
	  return false;

    for(int x=0; x<SHEET_SIZE; x++)
    for(int y=0; y<SHEET_SIZE; y++)
    {
        active[x][y] = false;
    }
  
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.x = seed[0];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.y = seed[1];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.z = seed[2];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2);
  
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.x = seed[0]+QUADMESH_SIZE*seed[3];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.y = seed[1]+QUADMESH_SIZE*seed[4];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.z = seed[2]+QUADMESH_SIZE*seed[5];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2+1);

    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.x = seed[0]-QUADMESH_SIZE*seed[3];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.y = seed[1]-QUADMESH_SIZE*seed[4];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.z = seed[2]-QUADMESH_SIZE*seed[5];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2-1);

    paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.x = seed[0]+QUADMESH_SIZE*seed[6];
    paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.y = seed[1]+QUADMESH_SIZE*seed[7];
    paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.z = seed[2]+QUADMESH_SIZE*seed[8];
    SetVectorField(SHEET_SIZE/2+1,SHEET_SIZE/2);

    paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.x = seed[0]-QUADMESH_SIZE*seed[6];
    paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.y = seed[1]-QUADMESH_SIZE*seed[7];
    paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.z = seed[2]-QUADMESH_SIZE*seed[8];
    SetVectorField(SHEET_SIZE/2-1,SHEET_SIZE/2);

    MakeActive(SHEET_SIZE/2,SHEET_SIZE/2);
    MakeActive(SHEET_SIZE/2,SHEET_SIZE/2+1);
    MakeActive(SHEET_SIZE/2,SHEET_SIZE/2-1);
    MakeActive(SHEET_SIZE/2+1,SHEET_SIZE/2);
    MakeActive(SHEET_SIZE/2-1,SHEET_SIZE/2);
    
    return true;
}


void PatchGenerator::OutputBoundary(Patch &boundary, pointSet &boundaryPoints, pointSet &boundaryPointsPaper)
{
  for(unsigned i = 0; i<boundaryPoints.size(); i++)
  {
	boundary.points.push_back(patchPoint(
		boundaryPointsPaper[i].x-SHEET_SIZE/2,
		boundaryPointsPaper[i].y-SHEET_SIZE/2,
		boundaryPoints[i].x,
		boundaryPoints[i].y,
		boundaryPoints[i].z));
  }
}

void PatchGenerator::OutputPatch(Patch &patch)
{  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
    {
		patch.points.push_back(patchPoint(x-SHEET_SIZE/2,
		                           y-SHEET_SIZE/2,
		                           paperSheet[x][y].pos.x,
		                           paperSheet[x][y].pos.y,
								   paperSheet[x][y].pos.z));
	}
}

int PatchGenerator::GeneratePatch(float seed[9],Patch &patch, Patch &boundary)
{ 
  int totPointsAdded = 0;
 
  activeListSize = 0;
  
  pointSet newPtsPaper;
  pointSet newPts;

  
  if (!SetSeed(seed))
  {
	  printf("Seed point is not a surface point");
	  return 0;
  }
  
  int i;
  int totIters=0;
  float f;  
  for(i = 0; i<MAX_GROWTH_STEPS; i++)
  {
    printf("#");
	fflush(stdout);
    int j = 0;
    while (((f=ForcesAndMove())>RELAX_FORCE_THRESHHOLD && j<MAX_RELAX_ITERATIONS) || j<MIN_RELAX_ITERATIONS)
	{
	  //printf("Largest force:%f\n",f);
      j++;
	}
	totIters += j;
	
	if (MarkHighStress())
	{
	  printf("\nHigh stress encountered\n");
	  break;
	}
	
    newPts.clear();
    newPtsPaper.clear();
	totPointsAdded += MakeNewPoints(newPts,newPtsPaper);
	AddNewPoints(newPts,newPtsPaper);
	
    if (totPointsAdded <10 && i==10)
	{
		printf("\nAborting, too few points added");
		break;
	}
  }
  
  printf("\n");
  printf("Growth steps:%d\n",i);
  printf("Mean relaxation iterations:%f\n",((float)totIters)/(float)i);
  
  OutputPatch(patch);
  OutputBoundary(boundary,newPts,newPtsPaper);
  
  return i;
}

