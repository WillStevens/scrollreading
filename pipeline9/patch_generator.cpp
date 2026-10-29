#include <iostream>
#include <cmath>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <utility>

#include <omp.h>
#include <smmintrin.h>

using namespace std;

#include "parameters.h"
#include "common_types.h"
#include "patch_generator.h"

#define MARGIN 8 // Don't try to fill near the edges of the volume

#define EXPECTED_DISTANCE(xd,yd) (QUADMESH_SIZE*sqrt(xd*xd+yd*yd))

#define NEIGHBOUR_RADIUS 2
#define NEIGHBOUR_RADIUS_FLOAT 2.0

void PatchGenerator::MakeActive(int x, int y)
{
	act(x,y) = true;
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
		distanceLookup[p] = 255-ZARRRead_1(surfaceZarr,zp,yp,xp);
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
  uint64_t px = (uint64_t)sheet(x,y).pos.x;
  uint64_t py = (uint64_t)sheet(x,y).pos.y;
  uint64_t pz = (uint64_t)sheet(x,y).pos.z;
  uint64_t p = (px<<32)+(py<<16)+pz;
  
  auto it = vectorFieldLookup.find(p);  
  if (it == vectorFieldLookup.end())
  {
	Vec3 v; // defaults to 0,0,0

    if (px-VOL_OFFSET_X>=0 && px-VOL_OFFSET_X<VOL_SIZE_X && py-VOL_OFFSET_Y>=0 && py-VOL_OFFSET_Y<VOL_SIZE_Y && pz-VOL_OFFSET_Z>=0 && pz-VOL_OFFSET_Z<VOL_SIZE_Z)
    {	
		
		/*v.x = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,0)*VECTORFIELD_CONSTANT;
		v.y = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,1)*VECTORFIELD_CONSTANT;
		v.z = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,2)*VECTORFIELD_CONSTANT;*/
		vfc->GetSmoothedVectorFieldInt8(px,py,pz,v);
		v = v*VECTORFIELD_CONSTANT;
	}

	vectorFieldLookup[p] = sheet(x,y).vectorField = v;
  }
  else
  {
	sheet(x,y).vectorField = it->second;
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
/*
		if (actualDistance>10) \
		{ \
		  printf("Distance error at: %d,%d %d,%d %f %f %f\n",x,y,xd,yd,sheet(x+xd,y+yd).pos.x,sheet(x+xd,y+yd).pos.y,sheet(x+xd,y+yd).pos.z);\
		} \
*/

#define FORCES_INNERLOOP(xd,yd) \
      if (act(x+xd,y+yd)) \
	  { \
		Vec3 direction = posxy-sheet(x+xd,y+yd).pos; \
        float actualDistance = direction.length(); \
        direction /= actualDistance; \
        \
        float force = expectedDistanceLookup[xd+1][yd+1]-actualDistance; \
        \
        acc += direction*force; \
	  }

float PatchGenerator::ForcesAndMove(float fieldWeight)
{  
  float largestForce = 0.0;
  
// This makes it slower rather than faster  
//#pragma omp parallel for reduction(max:largestForce)
  for(int ai = 0; ai<activeListSize; ai++)
  {
	int x = activeList[ai][0], y = activeList[ai][1];
	
    Vec3 acc = sheet(x,y).vectorField * fieldWeight; // This will get multiplie by springForceConstant later
	                                                       // So we have already divided by that in the constant definition for
											               // VECTORFIELD_CONSTANT
	Vec3 posxy = sheet(x,y).pos;

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
		
    largestForce = std::max(forceMag,largestForce);
  
    sheet(x,y).vel *= FRICTION_CONSTANT;
	
    sheet(x,y).vel += acc;
  }

  for(int ai = 0; ai<activeListSize; ai++)
  {
	int x = activeList[ai][0], y = activeList[ai][1];

    point oldPos = sheet(x,y).pos;
    sheet(x,y).pos += sheet(x,y).vel;
		
	if (int(oldPos.x) != int(sheet(x,y).pos.x) || int(oldPos.y) != int(sheet(x,y).pos.y) || int(oldPos.z) != int(sheet(x,y).pos.z))
	  SetVectorField(x,y);
  }
  
  return sqrt(largestForce);
}

bool PatchGenerator::TryToFill(int xp, int yp, Vec3 &rp)
{
  if (!act(xp,yp))
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
      if (act(xo1,yo1))
	  {
		  printf("%d,%d has active neighbour %d %d\n",xp,yp,xo1,yo1);
		  printf("%d %d status = %s\n",xo2,yo2,act(xo2,yo2)?"active":"inactive");
		  printf("%d %d status = %s\n",xc1,yc1,act(xc1,yc1)?"active":"inactive");
		  printf("%d %d status = %s\n",xd1,yd1,act(xd1,yd1)?"active":"inactive");
		  printf("%d %d status = %s\n",xa1,ya1,act(xa1,ya1)?"active":"inactive");
	  }
*/	  
      // If xo2,yo2 is in bound, then no need to check xo1,yo1
	  if (xo2>=0 && xo2<SHEET_SIZE && yo2>=0 && yo2<SHEET_SIZE && act(xo1,yo1) && act(xo2,yo2) && xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && xd1>=0 && xd1<SHEET_SIZE && yd1>=0 && yd1<SHEET_SIZE && act(xc1,yc1) && act(xd1,yd1))
	  {
		// Adjacent point, point beyond that, and points either side of adjacent point are all active
		rp = 2*sheet(xo1,yo1).pos - sheet(xo2,yo2).pos;
		//printf("Straightfill %f %f %f\n",rp.x,rp.y,rp.z);
		return true;
	  }
      // If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if (xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && act(xo1,yo1) && act(xa1,ya1) && act(xc1,yc1))
	  {
          // Get midpoint of xo1,yo1,xa1,ya1
		  rp = (sheet(xo1,yo1).pos+sheet(xa1,ya1).pos)/2;
          // Extend from the corner through the midpoint
          rp = 2*rp - sheet(xc1,yc1).pos;
		  //printf("Cornerfill %f %f %f\n",rp.x,rp.y,rp.z);
		  return true;
	  }
	}
  }
  return false;
}

void PatchGenerator::ClearHighStress(void)
{
	for(int x = 0; x<VOL_SIZE_X/STRESS_BLOCK_SIZE; x++)
	for(int y = 0; y<VOL_SIZE_Y/STRESS_BLOCK_SIZE; y++)
	for(int z = 0; z<VOL_SIZE_Z/STRESS_BLOCK_SIZE; z++)
		highStress[z][y][x] = 0;
}

bool PatchGenerator::MarkHighStress(void)
{  
  bool r = false;
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (act(x,y))
	{
	  float SETot = 0;
	  int SENum = 0;
	  // Don't bother with range check - make sure elsewhere that edges of sheet are not approached
      for(int xo=x-1; xo<x+2; xo++)
      for(int yo=y-1; yo<y+2; yo++)
        if (act(xo,yo) && ! (xo == x && yo == y)) 
		{
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            Vec3 direction = sheet(x,y).pos-sheet(xo,yo).pos;
            float actualDistance = direction.length();
                
            float force = (expectedDistance - actualDistance);
            SETot += force*force;
			SENum += 1;
		}
		
	  if (SETot/SENum > HIGH_STRESS_THRESHHOLD)
	  {
	    // paperPos is in x,y,z order
		int xb = (int)((sheet(x,y).pos.x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE);
		int yb = (int)((sheet(x,y).pos.y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE);
		int zb = (int)((sheet(x,y).pos.z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE);
		
		for(int xo = xb-(xb>0); xo <= xb+(xb+1<VOL_SIZE_X/STRESS_BLOCK_SIZE); xo++)
		for(int yo = yb-(yb>0); yo <= yb+(yb+1<VOL_SIZE_Y/STRESS_BLOCK_SIZE); yo++)
		for(int zo = zb-(zb>0); zo <= zb+(zb+1<VOL_SIZE_Z/STRESS_BLOCK_SIZE); zo++)
 		{
			highStress[zo][yo][xo] = true;
			r = true;
		}
		
		if (!silent) printf("\nHigh stress at x,y,z=%f,%f,%f",sheet(x,y).pos.x,sheet(x,y).pos.y,sheet(x,y).pos.z);
	  }
	}
	
	return r;
}

bool PatchGenerator::HasHighStress(int x, int y, int z)
{
	return highStress[(z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE][(y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE][(x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE];
}

// TODO newPtsPaper needs to be vector of int vec2
int PatchGenerator::MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper, bool ignoreSurfacePred)
{
    Vec3 np;

	ClearPointLookup();
    for(int x = 0; x<SHEET_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE; y++)
    {
		if (act(x,y))
			AddPointToLookup(point(sheet(x,y).pos));
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
		  
		  if (!act(x,y)) inactiveNeighbours.insert(pair<int,int>(x,y));
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
				  //printf("1");
				  okayToFill = false;
			  }
			  else if ((dist=GetDistanceAtPoint(px,py,pz))>0 && !ignoreSurfacePred)
			  {
				  //printf("2:%f:",dist);
				  okayToFill = false;
			  }
			  else if (HasCloseNeighbour(np))
			  {
				  //printf("3");
				  okayToFill = false;
			  }
			  
			  if (okayToFill)
			  {
				//printf(":New %d,%d:",x,y);  
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
	  sheet(x,y).pos = newPts[i];

	  sheet(x,y).vel.x = 0.0;
	  sheet(x,y).vel.y = 0.0;
	  sheet(x,y).vel.z = 0.0;
	  
      MakeActive(x,y);
	  SetVectorField(x,y);
    }
}

PatchGenerator::PatchGenerator(const string &surfaceZarrName_, size_t sheet_size_)
    : SHEET_SIZE((int)sheet_size_),
      surfaceZarrName(surfaceZarrName_),
      vfc(NULL),
      paperSheet(sheet_size_ * sheet_size_),
      activeList(sheet_size_ * sheet_size_),
      activeListSize(0),
      active(sheet_size_ * sheet_size_, 0)
{
	if (!silent) printf("Init\n");

    InitExpectedDistanceLookup();
	
    activeListSize = 0;	
}
		
PatchGenerator::~PatchGenerator(void)
{
}

bool PatchGenerator::SetSeed(const std::vector<float> &seed)
{  
    if (ZARRRead_1(surfaceZarr,seed[2],seed[1],seed[0]) != 255)
	  return false;

    for(int x=0; x<SHEET_SIZE; x++)
    for(int y=0; y<SHEET_SIZE; y++)
    {
        act(x,y) = false;
    }
  
    sheet(SHEET_SIZE/2,SHEET_SIZE/2).pos.x = seed[0];
    sheet(SHEET_SIZE/2,SHEET_SIZE/2).pos.y = seed[1];
    sheet(SHEET_SIZE/2,SHEET_SIZE/2).pos.z = seed[2];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2);
  
    sheet(SHEET_SIZE/2,SHEET_SIZE/2+1).pos.x = seed[0]+QUADMESH_SIZE*seed[3];
    sheet(SHEET_SIZE/2,SHEET_SIZE/2+1).pos.y = seed[1]+QUADMESH_SIZE*seed[4];
    sheet(SHEET_SIZE/2,SHEET_SIZE/2+1).pos.z = seed[2]+QUADMESH_SIZE*seed[5];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2+1);

    sheet(SHEET_SIZE/2,SHEET_SIZE/2-1).pos.x = seed[0]-QUADMESH_SIZE*seed[3];
    sheet(SHEET_SIZE/2,SHEET_SIZE/2-1).pos.y = seed[1]-QUADMESH_SIZE*seed[4];
    sheet(SHEET_SIZE/2,SHEET_SIZE/2-1).pos.z = seed[2]-QUADMESH_SIZE*seed[5];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2-1);

    sheet(SHEET_SIZE/2+1,SHEET_SIZE/2).pos.x = seed[0]+QUADMESH_SIZE*seed[6];
    sheet(SHEET_SIZE/2+1,SHEET_SIZE/2).pos.y = seed[1]+QUADMESH_SIZE*seed[7];
    sheet(SHEET_SIZE/2+1,SHEET_SIZE/2).pos.z = seed[2]+QUADMESH_SIZE*seed[8];
    SetVectorField(SHEET_SIZE/2+1,SHEET_SIZE/2);

    sheet(SHEET_SIZE/2-1,SHEET_SIZE/2).pos.x = seed[0]-QUADMESH_SIZE*seed[6];
    sheet(SHEET_SIZE/2-1,SHEET_SIZE/2).pos.y = seed[1]-QUADMESH_SIZE*seed[7];
    sheet(SHEET_SIZE/2-1,SHEET_SIZE/2).pos.z = seed[2]-QUADMESH_SIZE*seed[8];
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
  vector<patchPoint> points;

  for(unsigned i = 0; i<boundaryPoints.size(); i++)
  {
	points.push_back(patchPoint(
		boundaryPointsPaper[i].x-SHEET_SIZE/2,
		boundaryPointsPaper[i].y-SHEET_SIZE/2,
		boundaryPoints[i].x,
		boundaryPoints[i].y,
		boundaryPoints[i].z));
  }
  
  boundary.BuildFromPoints(points,0);
}

void PatchGenerator::OutputPatch(Patch &patch, int iter)
{ 
  vector<patchPoint> points;
 
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (act(x,y))
    {
		points.push_back(patchPoint(x-SHEET_SIZE/2,
		                           y-SHEET_SIZE/2,
		                           sheet(x,y).pos.x,
		                           sheet(x,y).pos.y,
								   sheet(x,y).pos.z));
	}
	
  if (points.size()==0)
  {
	  printf("Error - PatchGenerator::OutputPatch has encountered points.size()==0\n");
  }
  else
  {
	  if (!silent) printf("PatchGenerator::OutputPatch generated %d points\n",(int)points.size());
  }	  
  
  // Debug individual patch problem
/*  if (iter==347)
  {
	  FILE *f = fopen("d:/pipelineOutput/debug.csv","w");
	  for(auto &p : points)
	  {
		  fprintf(f,"%f,%f %f,%f,%f\n",p.x,p.y,p.v.x,p.v.y,p.v.z);
	  }
	  fclose(f);
	  exit(-1);
  }
  */
  patch.BuildFromPoints(points,iter);
  
  patch.SetPatchNum(iter);
}

int PatchGenerator::GeneratePatch(const std::vector<float> &seed,Patch &patch, Patch &boundary, int iter, bool _silent)
{ 
  silent = _silent;
  if (!silent) printf("Called GeneratePatch\n");
  int totPointsAdded = 0;
 
  activeListSize = 0;
  
  ClearHighStress();
  ClearPointLookup();
  distanceLookup.clear();

  if (iter%1000==0)
      vectorFieldLookup.clear();
  
  pointSet newPtsPaper;
  pointSet newPts;

  // TODO - in future it would be better to keep the zarrs open, but have more efficient buffer lookup
  if (!silent) printf("Opening zarrs\n");
  surfaceZarr = ZARROpen_1(surfaceZarrName.c_str()); 
  vfc = new VectorFieldCalculator(surfaceZarr);
  
  if (!SetSeed(seed))
  {
	  printf("Seed point is not a surface point\n");
	  delete vfc;
      ZARRClose_1(surfaceZarr);
	  return 0;
  }
  
  int i;
  int totIters=0;
  float f;  
  for(i = 0; i<MAX_GROWTH_STEPS; i++)
  {
    if (!silent) printf("#");
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
	  if (!silent) printf("\nHigh stress encountered\n");
	  break;
	}
	
    newPts.clear();
    newPtsPaper.clear();
	totPointsAdded += MakeNewPoints(newPts,newPtsPaper);
	AddNewPoints(newPts,newPtsPaper);
	
    if (totPointsAdded <10 && i==10)
	{
		if (!silent) printf("\nAborting, too few points added");
		break;
	}
  }
 
  if (!silent)
  { 
    printf("\n");
    printf("Growth steps:%d\n",i);
    printf("Mean relaxation iterations:%f\n",((float)totIters)/(float)i);
  }
  
  OutputPatch(patch,iter);
  
  if (newPts.size()>0)
	OutputBoundary(boundary,newPts,newPtsPaper);

  delete vfc;
  ZARRClose_1(surfaceZarr);
  
  return i;
}

void PatchGenerator::Dump(std::ostream &os)
{
  os << "FRAME" << std::endl;
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (act(x,y))
    {
		os << x-SHEET_SIZE/2 << "," << y-SHEET_SIZE/2 << "," << sheet(x,y).pos.x << "," << sheet(x,y).pos.y << "," << sheet(x,y).pos.z << std::endl;
	}
		
}

// Load a surface, ready to interpolate into any blank regions
// The surface should have integer surface coords and should ideally be a rectangle with holes in
// The output uses the same integer coordinate system
void PatchGenerator::FillHoles(const std::string &fileName, const std::string &outputFileName)
{
	silent = false;
    std::ofstream dumpFile("c:\\vesuvius\\temp\\dump.csv");	

    if (!silent) printf("Opening zarrs\n");
    surfaceZarr = ZARROpen_1(surfaceZarrName.c_str()); 
    vfc = new VectorFieldCalculator(surfaceZarr);

    int xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    int gridXmin = 0, gridXmax = 0, gridYmin = 0, gridYmax = 0;

	bool first = true;

    for(int x=0; x<SHEET_SIZE; x++)
    for(int y=0; y<SHEET_SIZE; y++)
    {
        act(x,y) = false;
    }
	
    for(int pass = 0; pass<2; pass++)
	{
        printf("Loading, pass %d\n",pass);
		
		FILE *fi = fopen(fileName.c_str(),"r");

		if(fi)
		{
			gridPointStruct p;
			fseek(fi,0,SEEK_END);
			long fsize = ftell(fi);
			fseek(fi,0,SEEK_SET);
  
			while(ftell(fi)<fsize)
			{
				fread(&p,sizeof(p),1,fi);
	
	            if (pass==0)
				{
					if (first || p.x < xmin)
						xmin = p.x;
					if (first || p.x > xmax)
						xmax = p.x;
					if (first || p.y < ymin)
						ymin = p.y;
					if (first || p.y > ymax)
						ymax = p.y;
					
					first=false;
				}
				else
				{
					int px = p.x - xmin - (xmax-xmin)/2+SHEET_SIZE/2;
					int py = p.y - ymin - (ymax-ymin)/2+SHEET_SIZE/2;
					
					//printf("%d,%d %f,%f,%f\n",px,py,p.px,p.py,p.pz);
					
					sheet(px,py).pos.x = p.px;
					sheet(px,py).pos.y = p.py;
					sheet(px,py).pos.z = p.pz;

					sheet(px,py).vel.x = 0.0;
					sheet(px,py).vel.y = 0.0;
					sheet(px,py).vel.z = 0.0;
	  
					MakeActive(px,py);
					SetVectorField(px,py);
				}				
			}
		
			fclose(fi);
		}
	}

	gridXmin = xmin - xmin - (xmax-xmin)/2+SHEET_SIZE/2;
	gridYmin = ymin - ymin - (ymax-ymin)/2+SHEET_SIZE/2;
	gridXmax = xmax - xmin - (xmax-xmin)/2+SHEET_SIZE/2;
	gridYmax = ymax - ymin - (ymax-ymin)/2+SHEET_SIZE/2;

	Dump(dumpFile);

	pointSet newPtsPaper;
	pointSet newPts;
	
	printf("About to start main loop\n");
	
	for(int i = 0; i<60; i++)
	{
		if (!silent) printf("#");
		fflush(stdout);
		int j = 0;
		float f;
		while (((f=ForcesAndMove(0.1))>RELAX_FORCE_THRESHHOLD && j<MAX_RELAX_ITERATIONS) || j<MIN_RELAX_ITERATIONS)
		{
			j++;
		}
		
		newPts.clear();
		newPtsPaper.clear();
		// ignoreSurfacePred = true because we want to make points outside of where there are surface predictions
		int pointsAdded = MakeNewPoints(newPts,newPtsPaper,true);
		
		printf("Made %d points\n",pointsAdded);
		
		decltype(newPts) keptPts;
		decltype(newPtsPaper) keptPtsPaper;
		
		// Only keep points that are within the original bounding box
		for(int i = 0; i<newPts.size(); i++)
		{
			if (newPtsPaper[i].x>=gridXmin && newPtsPaper[i].x<=gridXmax && newPtsPaper[i].y>=gridYmin && newPtsPaper[i].y<gridYmax)
			{
				keptPts.push_back(newPts[i]);
				keptPtsPaper.push_back(newPtsPaper[i]);
			}
		}

		printf("Kept %d points\n",(int)keptPts.size());

		if (keptPts.size()==0)
		{
			if (!silent) printf("\nNo more points added");
		}
		
		AddNewPoints(keptPts,keptPtsPaper);
		
		Dump(dumpFile);
	}

    // Save filled surface
	{
		FILE *fo = fopen(outputFileName.c_str(),"w");

		if(fo)
		{
			gridPointStruct p;
  
			for(int x = 0; x<SHEET_SIZE; x++)
			for(int y = 0; y<SHEET_SIZE; y++)
				if (act(x,y))
				{
					p.x= x - SHEET_SIZE/2 + (xmax-xmin)/2 + xmin;
					p.y= y - SHEET_SIZE/2 + (ymax-ymin)/2 + ymin;
					p.px = sheet(x,y).pos.x;
					p.py = sheet(x,y).pos.y;
					p.pz = sheet(x,y).pos.z;
					fwrite(&p,sizeof(p),1,fo);
				}
							
			fclose(fo);
		}
	}
  
    delete vfc;
    ZARRClose_1(surfaceZarr);
}
