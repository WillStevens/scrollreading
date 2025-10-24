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

#include "zarr_c64i1b256.c"

using namespace std;

#include "parameters.h"

/* These are now in parameters.h
#define VOL_OFFSET_X 2688
#define VOL_OFFSET_Y 1536
#define VOL_OFFSET_Z 4608

#define VF_SIZE_X 2048
#define VF_SIZE_Y 2560
#define VF_SIZE_Z 4096
*/

#define MARGIN 8 // Don't try to fill near the edges of the volume
#define SHEET_SIZE (MAX_GROWTH_STEPS+5)

/* These are now all defined in parameters.h - deleted after testing */
//#define SPRING_FORCE_CONSTANT 0.025f
/*
#define SPRING_FORCE_CONSTANT 0.05f
#define BEND_FORCE_CONSTANT 0.025f
#define FRICTION_CONSTANT 0.6f
#define VECTORFIELD_CONSTANT ((0.05f/73.0f)/SPRING_FORCE_CONSTANT) // factor of 1/73.0f was added when changing to single-byte vector field 
                                                                   // Divide by spring force sontant to make inner loop of Forces faster
#define RELAX_FORCE_THRESHHOLD 0.01f
#define MAX_RELAX_ITERATIONS 100
*/
#define EXPECTED_DISTANCE(xd,yd) (QUADMESH_SIZE*sqrt(xd*xd+yd*yd))

typedef union {
	__m128 m;
	float f[4];
} vect4;

typedef tuple<float,float,float> point;
typedef tuple<int,int,int> pointInt;
typedef vector<point> pointSet;

map< pointInt, pointSet > pointLookup;

map< pointInt, float> distanceLookup;

typedef unordered_map< uint64_t, vect4> vectorFieldLookupType;
vectorFieldLookupType vectorFieldLookup;

float seeds[1][9];

#define NEIGHBOUR_RADIUS 2
#define NEIGHBOUR_RADIUS_FLOAT 2.0

int dirVectorLookup[4][2] = { {1,0},{0,1},{-1,0},{0,-1}};

int activeList[SHEET_SIZE*SHEET_SIZE][2];
int activeListSize = 0;
bool active[SHEET_SIZE][SHEET_SIZE];

typedef struct {
	vect4 pos,vel,vectorField;
} paperPoint;

// Stored as x=0, y=1, z=2
paperPoint paperSheet[SHEET_SIZE][SHEET_SIZE] __attribute__((aligned(16)));

// If stress exceeds a threshhold, then mark the volume
// Don't need full resolution, so this shouldn't take up too much space
// e.g. if VF_SIZE is 2048 and STRESS_BLOCK_SIZE is 16 then this array takes up 2Mb
//#define HIGH_STRESS_THRESHHOLD 0.08
//#define HIGH_STRESS_THRESHHOLD 0.8
#define STRESS_BLOCK_SIZE 16
bool highStress[VOL_SIZE_Z/STRESS_BLOCK_SIZE][VOL_SIZE_Y/STRESS_BLOCK_SIZE][VOL_SIZE_X/STRESS_BLOCK_SIZE];

vect4 expectedDistanceLookup[3][3];

ZARR_c64i1b256 *vectorField;

void MakeActive(int x, int y)
{
	active[x][y] = true;
	activeList[activeListSize][0] = x;
	activeList[activeListSize++][1] = y;
}

float PointDistance(const point &p, const point &q)
{
	float xp = get<0>(p);
	float yp = get<1>(p);
	float zp = get<2>(p);
	float xq = get<0>(q);
	float yq = get<1>(q);
	float zq = get<2>(q);
	
	return sqrt((xp-xq)*(xp-xq)+(yp-yq)*(yp-yq)+(zp-zq)*(zp-zq));
}

void ClearPointLookup(void)
{
	pointLookup.clear();
}

void AddPointToLookup(const point &p)
{
	int xp = ((int)get<0>(p))/NEIGHBOUR_RADIUS;
	int yp = ((int)get<1>(p))/NEIGHBOUR_RADIUS;
	int zp = ((int)get<2>(p))/NEIGHBOUR_RADIUS;
	
	if (pointLookup.count(pointInt(xp,yp,zp))==0)
	  pointLookup[pointInt(xp,yp,zp)] = pointSet();
	pointLookup[pointInt(xp,yp,zp)].push_back(p);
}


float GetDistanceAtPoint(const pointInt &p)
{
	int xp = ((int)get<0>(p));
	int yp = ((int)get<1>(p));
	int zp = ((int)get<2>(p));

	if (distanceLookup.count(p) == 0)
	{
		distanceLookup[p] = ZARRRead_c64i1b256(vectorField,zp,yp,xp,3);
	}
	return distanceLookup[p];
}

bool HasCloseNeighbour(const point &p)
{
	int xp = ((int)get<0>(p))/NEIGHBOUR_RADIUS;
	int yp = ((int)get<1>(p))/NEIGHBOUR_RADIUS;
	int zp = ((int)get<2>(p))/NEIGHBOUR_RADIUS;
	
	for(int x = xp-1; x<=xp+1; x++)
	for(int y = yp-1; y<=yp+1; y++)
	for(int z = zp-1; z<=zp+2; z++)
	{
		for(const point &q : pointLookup[pointInt(x,y,z)])
		{
			if (PointDistance(p,q)<=NEIGHBOUR_RADIUS_FLOAT)
			{
				return true;
			}
		}
	}

	return false;
}

void SetVectorField(int x, int y)
{
  uint64_t px = (uint64_t)paperSheet[x][y].pos.f[0];
  uint64_t py = (uint64_t)paperSheet[x][y].pos.f[1];
  uint64_t pz = (uint64_t)paperSheet[x][y].pos.f[2];
  uint64_t p = (px<<32)+(py<<16)+pz;
  
  vectorFieldLookupType::iterator it = vectorFieldLookup.find(p);  
  if (it == vectorFieldLookup.end())
  {
	vect4 v;

    if (px-VOL_OFFSET_X>=0 && px-VOL_OFFSET_X<VOL_SIZE_X && py-VOL_OFFSET_Y>=0 && py-VOL_OFFSET_Y<VOL_SIZE_Y && pz-VOL_OFFSET_Z>=0 && pz-VOL_OFFSET_Z<VOL_SIZE_Z)
    {	
		v.f[0] = ZARRRead_c64i1b256(vectorField,pz,py,px,0)*VECTORFIELD_CONSTANT;
		v.f[1] = ZARRRead_c64i1b256(vectorField,pz,py,px,1)*VECTORFIELD_CONSTANT;
		v.f[2] = ZARRRead_c64i1b256(vectorField,pz,py,px,2)*VECTORFIELD_CONSTANT;
		v.f[3] = 0;
		
	}
	else
	{
		v.m = _mm_set1_ps(0.0);
	}

	vectorFieldLookup[p] = paperSheet[x][y].vectorField = v;
  }
  else
  {
	paperSheet[x][y].vectorField = it->second;
  }
  
  //printf("Vector field for %d,%d was %f,%f,%f,%f\n",x,y,paperVectorField[x][y][0],paperVectorField[x][y][1],paperVectorField[x][y][2],paperVectorField[x][y][3]);
}

bool InitialiseSeed(void)
{
  int currentSeed = 0; // Hangover from simpaper4.cpp - set to zero to avoid replacing lots of code below

  for(int x=0; x<SHEET_SIZE; x++)
  for(int y=0; y<SHEET_SIZE; y++)
  {
    active[x][y] = false;
  }
  
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.f[0] = seeds[currentSeed][0];
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.f[1] = seeds[currentSeed][1];
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.f[2] = seeds[currentSeed][2];
  SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2);
  
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.f[0] = seeds[currentSeed][0]+QUADMESH_SIZE*seeds[currentSeed][3];
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.f[1] = seeds[currentSeed][1]+QUADMESH_SIZE*seeds[currentSeed][4];
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.f[2] = seeds[currentSeed][2]+QUADMESH_SIZE*seeds[currentSeed][5];
  SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2+1);

  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.f[0] = seeds[currentSeed][0]-QUADMESH_SIZE*seeds[currentSeed][3];
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.f[1] = seeds[currentSeed][1]-QUADMESH_SIZE*seeds[currentSeed][4];
  paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.f[2] = seeds[currentSeed][2]-QUADMESH_SIZE*seeds[currentSeed][5];
  SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2-1);

  paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.f[0] = seeds[currentSeed][0]+QUADMESH_SIZE*seeds[currentSeed][6];
  paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.f[1] = seeds[currentSeed][1]+QUADMESH_SIZE*seeds[currentSeed][7];
  paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.f[2] = seeds[currentSeed][2]+QUADMESH_SIZE*seeds[currentSeed][8];
  SetVectorField(SHEET_SIZE/2+1,SHEET_SIZE/2);

  paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.f[0] = seeds[currentSeed][0]-QUADMESH_SIZE*seeds[currentSeed][6];
  paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.f[1] = seeds[currentSeed][1]-QUADMESH_SIZE*seeds[currentSeed][7];
  paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.f[2] = seeds[currentSeed][2]-QUADMESH_SIZE*seeds[currentSeed][8];
  SetVectorField(SHEET_SIZE/2-1,SHEET_SIZE/2);

  MakeActive(SHEET_SIZE/2,SHEET_SIZE/2);
  MakeActive(SHEET_SIZE/2,SHEET_SIZE/2+1);
  MakeActive(SHEET_SIZE/2,SHEET_SIZE/2-1);
  MakeActive(SHEET_SIZE/2+1,SHEET_SIZE/2);
  MakeActive(SHEET_SIZE/2-1,SHEET_SIZE/2);
    
  return true;
}

void InitExpectedDistanceLookup(void)
{
  for(int x = 0; x<3; x++)
  for(int y = 0; y<3; y++)
  {
    for(int i = 0; i<4; i++)
      expectedDistanceLookup[x][y].f[i] = QUADMESH_SIZE*sqrt((x-1)*(x-1)+(y-1)*(y-1));
  }
}

#define FORCES_INNERLOOP(xd,yd) \
      if (active[x+xd][y+yd]) \
	  { \
        __m128 expectedDistance = _mm_set1_ps(EXPECTED_DISTANCE(xd,yd)); \
		__m128 direction = _mm_sub_ps(posxy,paperSheet[x+xd][y+yd].pos.m); \
        __m128 dp = _mm_dp_ps(direction,direction,0xff); \
		__m128 actualDistance = _mm_sqrt_ps(dp); \
        direction = _mm_div_ps(direction,actualDistance); \
        \
        __m128 force = _mm_sub_ps(expectedDistance,actualDistance); \
        \
        acc = _mm_add_ps(acc,_mm_mul_ps(direction,force)); \
	  }

float ForcesAndMove(void)
{  
  float largestForce = 0.0;
  
  __m128 springForceConstant = _mm_set_ps1(SPRING_FORCE_CONSTANT);
  
  for(int ai = 0; ai<activeListSize; ai++)
  {
	int x = activeList[ai][0], y = activeList[ai][1];
	
    __m128 acc = paperSheet[x][y].vectorField.m; // This will get multiplie by springForceConstant later
	                                                              // So we have already divided by that in the constant definition for
																  // VECTORFIELD_CONSTANT
	__m128 posxy = paperSheet[x][y].pos.m;
	
	// Don't bother with range checks - make sure elsewhere that growth stops before we get to the edges
    FORCES_INNERLOOP(-1,-1)
	FORCES_INNERLOOP(-1,0)
	FORCES_INNERLOOP(-1,1)
	FORCES_INNERLOOP(0,-1)
	FORCES_INNERLOOP(0,1)
	FORCES_INNERLOOP(1,-1)
	FORCES_INNERLOOP(1,0)
	FORCES_INNERLOOP(1,1)

    acc = _mm_mul_ps(acc,springForceConstant);

    float forceMag = _mm_cvtss_f32(_mm_dp_ps(acc,acc,0x71));
		
    if (forceMag > largestForce)
      largestForce = forceMag;
  
    paperSheet[x][y].vel.m = _mm_mul_ps(paperSheet[x][y].vel.m,_mm_set1_ps(FRICTION_CONSTANT));
	
    paperSheet[x][y].vel.m = _mm_add_ps(paperSheet[x][y].vel.m,acc);
  }

  for(int ai = 0; ai<activeListSize; ai++)
  {
	int x = activeList[ai][0], y = activeList[ai][1];

    __m128 oldPos = _mm_load_ps(&paperSheet[x][y].pos.f[0]);
    paperSheet[x][y].pos.m = _mm_add_ps(oldPos,paperSheet[x][y].vel.m);
	oldPos = _mm_floor_ps(oldPos);
	oldPos = _mm_cmpeq_ps(oldPos,_mm_floor_ps(paperSheet[x][y].pos.m));
		
	if (_mm_movemask_ps(oldPos)!=0xf)
	  SetVectorField(x,y);
  }
  
  return sqrt(largestForce);
}

bool TryToFill(int xp, int yp, float &rp0, float &rp1, float &rp2)
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
        rp0 = 2*paperSheet[xo1][yo1].pos.f[0] - paperSheet[xo2][yo2].pos.f[0];
        rp1 = 2*paperSheet[xo1][yo1].pos.f[1] - paperSheet[xo2][yo2].pos.f[1];
        rp2 = 2*paperSheet[xo1][yo1].pos.f[2] - paperSheet[xo2][yo2].pos.f[2];
		//printf("Straightfill %f %f %f\n",rp0,rp1,rp2);
		return true;
	  }
      // If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if (xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && active[xo1][yo1] && active[xa1][ya1] && active[xc1][yc1])
	  {
          // Get midpoint of xo1,yo1,xa1,ya1
          rp0 = (paperSheet[xo1][yo1].pos.f[0]+paperSheet[xa1][ya1].pos.f[0])/2;
          rp1 = (paperSheet[xo1][yo1].pos.f[1]+paperSheet[xa1][ya1].pos.f[1])/2;
          rp2 = (paperSheet[xo1][yo1].pos.f[2]+paperSheet[xa1][ya1].pos.f[2])/2;
          // Extend from the corner through the midpoint
          rp0 = 2*rp0 - paperSheet[xc1][yc1].pos.f[0];
		  rp1 = 2*rp1 - paperSheet[xc1][yc1].pos.f[1];
		  rp2 = 2*rp2 - paperSheet[xc1][yc1].pos.f[2];
		  //printf("Cornerfill %f %f %f\n",rp0,rp1,rp2);
		  return true;
	  }
	}
  }
  return false;
}

bool MarkHighStress(void)
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
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1].f[0];
            float direction0 = paperSheet[x][y].pos.f[0]-paperSheet[xo][yo].pos.f[0];
            float direction1 = paperSheet[x][y].pos.f[1]-paperSheet[xo][yo].pos.f[1];
            float direction2 = paperSheet[x][y].pos.f[2]-paperSheet[xo][yo].pos.f[2];
            float actualDistance = sqrt(direction0*direction0+direction1*direction1+direction2*direction2);
                
            float force = (expectedDistance - actualDistance);
            SETot += force*force;
			SENum += 1;
		}
		
	  if (SETot/SENum > HIGH_STRESS_THRESHHOLD)
	  {
	    // paperPos is in x,y,z order
		int xb = (int)((paperSheet[x][y].pos.f[0]-VOL_OFFSET_X)/STRESS_BLOCK_SIZE);
		int yb = (int)((paperSheet[x][y].pos.f[1]-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE);
		int zb = (int)((paperSheet[x][y].pos.f[2]-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE);
		
		for(int xo = xb-(xb>0); xo <= xb+(xb+1<VOL_SIZE_X/STRESS_BLOCK_SIZE); xo++)
		for(int yo = yb-(yb>0); yo <= yb+(yb+1<VOL_SIZE_Y/STRESS_BLOCK_SIZE); yo++)
		for(int zo = zb-(zb>0); zo <= zb+(zb+1<VOL_SIZE_Z/STRESS_BLOCK_SIZE); zo++)
 		{
			highStress[zo][yo][xo] = true;
			r = true;
		}
		
		printf("\nHigh stress at x,y,z=%f,%f,%f",paperSheet[x][y].pos.f[0],paperSheet[x][y].pos.f[1],paperSheet[x][y].pos.f[2]);
	  }
	}
	
	return r;
}

bool HasHighStress(int x, int y, int z)
{
	return highStress[(z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE][(y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE][(x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE];
}

void Output(const char *filename)
{
  FILE *f = fopen(filename,"w");
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
	  // output in x,y,z order 
      fprintf(f,"%d,%d,%f,%f,%f\n",x-SHEET_SIZE/2,y-SHEET_SIZE/2,paperSheet[x][y].pos.f[0],paperSheet[x][y].pos.f[1],paperSheet[x][y].pos.f[2]); 
  fclose(f);
}

void BinBoundaryOutput(const char *filename, pointSet &boundaryPoints, pointSet &boundaryPointsPaper)
{
  FILE *f = fopen(filename,"w");
  
  struct __attribute__((packed)) {float x,y,px,py,pz;} p;
  
  for(unsigned i = 0; i<boundaryPoints.size(); i++)
  {
		p.x = std::get<0>(boundaryPointsPaper[i]);
		p.y = std::get<1>(boundaryPointsPaper[i]);
		p.px = std::get<0>(boundaryPoints[i]);
		p.py = std::get<1>(boundaryPoints[i]);
		p.pz = std::get<2>(boundaryPoints[i]);
		
		fwrite(&p,sizeof(p),1,f);
  }
	
  fclose(f);
}

void BinOutput(const char *filename)
{
  FILE *f = fopen(filename,"w");
  
  struct __attribute__((packed)) {float x,y,px,py,pz;} p;
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
    {
		p.x = x-SHEET_SIZE/2; p.y = y-SHEET_SIZE/2;
		p.px = paperSheet[x][y].pos.f[0];
		p.py = paperSheet[x][y].pos.f[1];
		p.pz = paperSheet[x][y].pos.f[2];
		
		fwrite(&p,sizeof(p),1,f);
	}
  fclose(f);
}

void StressOutput(const char *filename)
{
  FILE *f = fopen(filename,"w");
  
  float SESum = 0.0f;
  int SETotNum = 0;
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
	{
	  float SETot = 0;
	  int SENum = 0;
      for(int xo=(x-1>0?x-1:0); xo<(x+2<SHEET_SIZE?x+2:SHEET_SIZE); xo++)
      for(int yo=(y-1>0?y-1:0); yo<(y+2<SHEET_SIZE?y+2:SHEET_SIZE); yo++)
        if (active[xo][yo] && ! (xo == x && yo == y))
		{
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1].f[0];
            float direction0 = paperSheet[x][y].pos.f[0]-paperSheet[xo][yo].pos.f[0];
            float direction1 = paperSheet[x][y].pos.f[1]-paperSheet[xo][yo].pos.f[1];
            float direction2 = paperSheet[x][y].pos.f[2]-paperSheet[xo][yo].pos.f[2];
            float actualDistance = sqrt(direction0*direction0+direction1*direction1+direction2*direction2);
                
            float force = (expectedDistance - actualDistance);
            SETot += force*force;
            SESum += force*force;
			SENum += 1;
			SETotNum += 1;
		}
		
		fprintf(f,"%f\n",SETot/SENum);
	}
    else
      fprintf(f,"0\n");		
  fclose(f);
  
  printf("Strain energy: %f\n",SESum / SETotNum);
}

int MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper)
{
    float np0,np1,np2;

	ClearPointLookup();
    for(int x = 0; x<SHEET_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE; y++)
    {
		if (active[x][y])
			AddPointToLookup(point(paperSheet[x][y].pos.f[0],paperSheet[x][y].pos.f[1],paperSheet[x][y].pos.f[2]));
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
		  if (TryToFill(x,y,np0,np1,np2))
		  { 
			float dist;
			bool okayToFill = true;
			int px = (int)np0;
			int py = (int)np1;
			int pz = (int)np2;
			if (px-VOL_OFFSET_X>=MARGIN && px-VOL_OFFSET_X<VOL_SIZE_X-MARGIN && py-VOL_OFFSET_Y>=MARGIN && py-VOL_OFFSET_Y<VOL_SIZE_Y-MARGIN && pz-VOL_OFFSET_Z>=MARGIN && pz-VOL_OFFSET_Z<VOL_SIZE_Z-MARGIN)
			{
			  if (HasHighStress(px,py,pz))
			  {
				  //printf("1\n");
				  okayToFill = false;
			  }
			  else if ((dist=GetDistanceAtPoint(pointInt(px,py,pz)))>0)
			  {
				  //printf("2:%f\n",dist);
				  okayToFill = false;
			  }
			  else if (HasCloseNeighbour(point(np0,np1,np2)))
			  {
				  //printf("3\n");
				  okayToFill = false;
			  }
			  
			  if (okayToFill)
			  {
				//printf("New point at %d,%d\n",x,y);  
				newPtsPaper.push_back(point(x,y,0.0));
				newPts.push_back(point(np0,np1,np2));
			  }
			}
		  }
      
    }
    //printf("%d points added\n",(int)newPts.size());     

    return newPts.size();	
}

void AddNewPoints(pointSet &newPts, pointSet &newPtsPaper)
{
    for(int i = 0; i<(int)newPts.size(); i++)
    {
	  point paperPoint = newPtsPaper[i];
	  int x = get<0>(paperPoint);
	  int y = get<1>(paperPoint);
	  point scrollPoint = newPts[i];

      paperSheet[x][y].pos.f[0] = get<0>(scrollPoint);
      paperSheet[x][y].pos.f[1] = get<1>(scrollPoint);
      paperSheet[x][y].pos.f[2] = get<2>(scrollPoint);

	  paperSheet[x][y].vel.f[0] = 0.0;
	  paperSheet[x][y].vel.f[1] = 0.0;
	  paperSheet[x][y].vel.f[2] = 0.0;
	  
      MakeActive(x,y);
	  SetVectorField(x,y);
    }
}

int main(int argc, char *argv[])
{ 
  if (argc != 9+4)
  {
	printf("Usage: simpaper9 <seed> <vector field zarr> <surface points output> <boundary points>\n");
	printf("Where seed is x,y,z,ax,ay,az,bx,by,bz - coordinates and two vectors specifying orientation of plane\n");
	return -1;
  }

  for(int i=0; i<9; i++)
    seeds[0][i] = atof(argv[i+1]);

  int totPointsAdded = 0;
  
  pointSet newPtsPaper;
  pointSet newPts;

  vectorField = ZARROpen_c64i1b256(argv[9+1]);
  InitExpectedDistanceLookup();
  
  InitialiseSeed();
  
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
  printf("Mean relaxation iterations:%f\n",((float)totIters)/(float)i);
  
  BinOutput(argv[9+2]);
  BinBoundaryOutput(argv[9+3],newPts,newPtsPaper);
  
  ZARRClose_c64i1b256(vectorField);
  
  
  return i;
}

