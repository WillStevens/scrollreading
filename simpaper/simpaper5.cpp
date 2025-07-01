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

#include "../zarrlike/zarr_6.c"

using namespace std;

#define VOL_OFFSET_X 2688
#define VOL_OFFSET_Y 1536
#define VOL_OFFSET_Z 4608

#define VF_SIZE 2048
#define MARGIN 8 // Don't try to fill near the edges of the volume
#define SHEET_SIZE 1000

#define SPRING_FORCE_CONSTANT 0.025f
#define BEND_FORCE_CONSTANT 0.025f
#define FRICTION_CONSTANT 0.9f
#define VECTORFIELD_CONSTANT 0.05f

#define STEP_SIZE 3

typedef tuple<float,float,float> point;
typedef tuple<int,int,int> pointInt;
typedef vector<point> pointSet;

map< pointInt, pointSet > pointLookup;

map< pointInt, float> distanceLookup;

map< pointInt, point> vectorFieldLookup;

float seeds[1][9];

#define NEIGHBOUR_RADIUS 2
#define NEIGHBOUR_RADIUS_FLOAT 2.0

int dirVectorLookup[4][2] = { {1,0},{0,1},{-1,0},{0,-1}};

bool active[SHEET_SIZE][SHEET_SIZE];

// Stored as x=0, y=1, z=2
float paperPos[SHEET_SIZE][SHEET_SIZE][3];
float paperVel[SHEET_SIZE][SHEET_SIZE][3];
float paperAcc[SHEET_SIZE][SHEET_SIZE][3];
// Store the vector associated with the current point - update it if the point moves
float paperVectorField[SHEET_SIZE][SHEET_SIZE][3];

// If stress exceeds a threshhold, then mark the volume
// Don't need full resolution, so this shouldn't take up too much space
// e.g. if VF_SIZE is 2048 and STRESS_BLOCK_SIZE is 16 then this array takes up 2Mb
#define HIGH_STRESS_THRESHHOLD 0.05
#define STRESS_BLOCK_SIZE 16
bool highStress[VF_SIZE/STRESS_BLOCK_SIZE][VF_SIZE/STRESS_BLOCK_SIZE][VF_SIZE/STRESS_BLOCK_SIZE];

float expectedDistanceLookup[3][3];

ZARR_6 *vectorField;

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
		distanceLookup[p] = ZARRRead_6(vectorField,zp,yp,xp,3);
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
  int px = (int)paperPos[x][y][0];
  int py = (int)paperPos[x][y][1];
  int pz = (int)paperPos[x][y][2];
  if (px>=0 && px<VF_SIZE && py>=0 && py<VF_SIZE && pz>=0 && pz<VF_SIZE)
  {
	pointInt p(px,py,pz);
	
	if (vectorFieldLookup.count(p)==0)
	{
		vectorFieldLookup[p] = point(ZARRRead_6(vectorField,pz,py,px,0)*VECTORFIELD_CONSTANT,ZARRRead_6(vectorField,pz,py,px,1)*VECTORFIELD_CONSTANT,ZARRRead_6(vectorField,pz,py,px,2)*VECTORFIELD_CONSTANT);
	}
	
	point pf = vectorFieldLookup[p];
    paperVectorField[x][y][0] = std::get<0>(pf);
    paperVectorField[x][y][1] = std::get<1>(pf);
    paperVectorField[x][y][2] = std::get<2>(pf);
  }
  else
  {
    paperVectorField[x][y][0] = 0.0;
    paperVectorField[x][y][1] = 0.0;
    paperVectorField[x][y][2] = 0.0;
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
  
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2][0] = seeds[currentSeed][0];
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2][1] = seeds[currentSeed][1];
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2][2] = seeds[currentSeed][2];
  SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2);
  
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2+1][0] = seeds[currentSeed][0]+seeds[currentSeed][3];
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2+1][1] = seeds[currentSeed][1]+seeds[currentSeed][4];
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2+1][2] = seeds[currentSeed][2]+seeds[currentSeed][5];
  SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2+1);

  paperPos[SHEET_SIZE/2][SHEET_SIZE/2-1][0] = seeds[currentSeed][0]-seeds[currentSeed][3];
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2-1][1] = seeds[currentSeed][1]-seeds[currentSeed][4];
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2-1][2] = seeds[currentSeed][2]-seeds[currentSeed][5];
  SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2-1);

  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2][0] = seeds[currentSeed][0]+seeds[currentSeed][6];
  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2][1] = seeds[currentSeed][1]+seeds[currentSeed][7];
  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2][2] = seeds[currentSeed][2]+seeds[currentSeed][8];
  SetVectorField(SHEET_SIZE/2+1,SHEET_SIZE/2);

  paperPos[SHEET_SIZE/2-1][SHEET_SIZE/2][0] = seeds[currentSeed][0]-seeds[currentSeed][6];
  paperPos[SHEET_SIZE/2-1][SHEET_SIZE/2][1] = seeds[currentSeed][1]-seeds[currentSeed][7];
  paperPos[SHEET_SIZE/2-1][SHEET_SIZE/2][2] = seeds[currentSeed][2]-seeds[currentSeed][8];
  SetVectorField(SHEET_SIZE/2-1,SHEET_SIZE/2);

  active[SHEET_SIZE/2][SHEET_SIZE/2] = true;
  active[SHEET_SIZE/2][SHEET_SIZE/2+1] = true;
  active[SHEET_SIZE/2][SHEET_SIZE/2-1] = true;
  active[SHEET_SIZE/2+1][SHEET_SIZE/2] = true;
  active[SHEET_SIZE/2-1][SHEET_SIZE/2] = true;
    
  return true;
}

void InitExpectedDistanceLookup(void)
{
  for(int x = 0; x<3; x++)
  for(int y = 0; y<3; y++)
  {
    expectedDistanceLookup[x][y] = STEP_SIZE*sqrt((x-1)*(x-1)+(y-1)*(y-1));
  }
}

float Forces(void)
{  
  float largestForce = 0.0;
  
  for(int x=0; x<SHEET_SIZE; x++)
	for(int y = 0; y<SHEET_SIZE; y++)
    {
      if (active[x][y])
	  {
        int px = (int)paperPos[x][y][0];
		int py = (int)paperPos[x][y][1];
		int pz = (int)paperPos[x][y][2];
        if (px>=0 && px<VF_SIZE && py>=0 && py<VF_SIZE && pz>=0 && pz<VF_SIZE)
		{
          paperAcc[x][y][0] = paperVectorField[x][y][0];
          paperAcc[x][y][1] = paperVectorField[x][y][1];
          paperAcc[x][y][2] = paperVectorField[x][y][2];
        }
		else
		{
          paperAcc[x][y][0] = 0.0;
          paperAcc[x][y][1] = 0.0;
          paperAcc[x][y][2] = 0.0;
		}
        for(int xo=(x-1>0?x-1:0); xo<(x+2<SHEET_SIZE?x+2:SHEET_SIZE); xo++)
        for(int yo=(y-1>0?y-1:0); yo<(y+2<SHEET_SIZE?y+2:SHEET_SIZE); yo++)
          if (active[xo][yo] && ! (xo == x && yo == y))
		  {
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            float direction0 = paperPos[x][y][0]-paperPos[xo][yo][0];
            float direction1 = paperPos[x][y][1]-paperPos[xo][yo][1];
            float direction2 = paperPos[x][y][2]-paperPos[xo][yo][2];
            float actualDistance = sqrt(direction0*direction0+direction1*direction1+direction2*direction2);
            direction0 /= actualDistance;
            direction1 /= actualDistance;
            direction2 /= actualDistance;
                
            float force = (expectedDistance - actualDistance)*SPRING_FORCE_CONSTANT;
                
            paperAcc[x][y][0] += direction0 * force;
            paperAcc[x][y][1] += direction1 * force;
            paperAcc[x][y][2] += direction2 * force;
		  }
		  
#if 0		  
		// If particles two steps away are too close, fend them off - resist bending too tightly
        static int twoPositions[6][3] = {{-2,0},{2,0},{0,-2},{0,2}};
        for(int i=0; i<4; i++)
		{
			int xo = x+twoPositions[i][0];
			int yo = y+twoPositions[i][1];
			
			if (xo>=0 && xo<SHEET_SIZE && yo>=0 && yo<SHEET_SIZE)
			{
              float direction0 = paperPos[x][y][0]-paperPos[xo][yo][0];
              float direction1 = paperPos[x][y][1]-paperPos[xo][yo][1];
              float direction2 = paperPos[x][y][2]-paperPos[xo][yo][2];
              float actualDistance = sqrt(direction0*direction0+direction1*direction1+direction2*direction2);
              direction0 /= actualDistance;
              direction1 /= actualDistance;
              direction2 /= actualDistance;
                
			  if (actualDistance < 1.9f*STEP_SIZE)
			  {
                float force = (1.9f*STEP_SIZE - actualDistance)*BEND_FORCE_CONSTANT;
                
                paperAcc[x][y][0] += direction0 * force;
                paperAcc[x][y][1] += direction1 * force;
                paperAcc[x][y][2] += direction2 * force;
			  }
			}
		}
#endif		
        float forceMag = sqrt(paperAcc[x][y][0]*paperAcc[x][y][0]+paperAcc[x][y][1]*paperAcc[x][y][1]+paperAcc[x][y][2]*paperAcc[x][y][2]);
		
        if (forceMag > largestForce)
          largestForce = forceMag;
      }
	}
	
  for(int x=0; x<SHEET_SIZE; x++)
  for(int y=0; y<SHEET_SIZE; y++)
  {
	paperVel[x][y][0] *= FRICTION_CONSTANT;
	paperVel[x][y][1] *= FRICTION_CONSTANT;
	paperVel[x][y][2] *= FRICTION_CONSTANT;
  }
  return largestForce;
}

/*	  
def DrawIt(i):
  # Output the points
  pointsOut = np.zeros((50,50,50))

  for x in range(0,50):
    for y in range(0,50):
      pointsOut[25,y,x] = np.linalg.norm(vectorField[25,y,x])/5.0
      pointsOut[10,y,x] = np.linalg.norm(vectorField[10,y,x])/5.0
      pointsOut[40,y,x] = np.linalg.norm(vectorField[40,y,x])/5.0

  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      (px,py,pz) = (paperPos[x,y,0],paperPos[x,y,1],paperPos[x,y,2])
      (px,py,pz) = (int(px),int(py),int(pz))
      if pz>=0 and pz<50 and py>=0 and py<50 and px>=0 and px<50:
        pointsOut[pz,py,px] = 1.0
    

  j = Image.fromarray(pointsOut[25,:,:])
  j.save("simpaper_out\\progress_25_%03d.tif"%i)
  j = Image.fromarray(pointsOut[10,:,:])
  j.save("simpaper_out\\progress_10_%03d.tif"%i)
  j = Image.fromarray(pointsOut[40,:,:])
  j.save("simpaper_out\\progress_40_%03d.tif"%i)
  
def PlotIt(i):
        
  # Change the Size of Graph using 
  # Figsize
  fig = plt.figure(figsize=(20, 20))

  ax = plt.axes(projection='3d')

  ax.set_xlim([-1,51])
  ax.set_ylim([-1,51])
  ax.set_zlim([-1,51])
  
  # Creating array points

  
  xList = []
  yList = []
  zList = []
  for x in range(0,paperSize[0]):
    for y in range(0,paperSize[1]):
      xList += [paperPos[x,y,0]]
      yList += [paperPos[x,y,1]]
      zList += [paperPos[x,y,2]]
      
  xa = np.array(xList)
  ya = np.array(yList)
  za = np.array(zList)

  # To create a scatter graph
  ax.scatter(xa,ya,za)

  """
  ax.plot_surface(paperPos[:,:,0],paperPos[:,:,1],paperPos[:,:,2])
  """
  """
  x = np.linspace(0,50,50)
  y = np.linspace(0,50,50)
  z = np.linspace(0,50,50)
  
  X,Y,Z = np.meshgrid(x,y,z)
  
  ax.quiver(X,Y,Z,vectorField[:,:,:,0],vectorField[:,:,:,1],vectorField[:,:,:,2])
  """
  # turn off/on axis
  plt.axis('on')

  plt.savefig("simpaper_out//simpaper_test_%05i.png" % i,bbox_inches='tight')

  plt.close()
  # show the graph
  #plt.show()
*/
/* TODO - if more than one possible coordinate, take the average of all? */          

/*              x
    x   xxx    xxx
   xxx  xxx   xxxxx
    x   xxx    xxx
                x
*/
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
        rp0 = 2*paperPos[xo1][yo1][0] - paperPos[xo2][yo2][0];
        rp1 = 2*paperPos[xo1][yo1][1] - paperPos[xo2][yo2][1];
        rp2 = 2*paperPos[xo1][yo1][2] - paperPos[xo2][yo2][2];
		//printf("Straightfill %f %f %f\n",rp0,rp1,rp2);
		return true;
	  }
      // If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if (xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && active[xo1][yo1] && active[xa1][ya1] && active[xc1][yc1])
	  {
          // Get midpoint of xo1,yo1,xa1,ya1
          rp0 = (paperPos[xo1][yo1][0]+paperPos[xa1][ya1][0])/2;
          rp1 = (paperPos[xo1][yo1][1]+paperPos[xa1][ya1][1])/2;
          rp2 = (paperPos[xo1][yo1][2]+paperPos[xa1][ya1][2])/2;
          // Extend from the corner through the midpoint
          rp0 = 2*rp0 - paperPos[xc1][yc1][0];
		  rp1 = 2*rp1 - paperPos[xc1][yc1][1];
		  rp2 = 2*rp2 - paperPos[xc1][yc1][2];
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
      for(int xo=(x-1>0?x-1:0); xo<(x+2<SHEET_SIZE?x+2:SHEET_SIZE); xo++)
      for(int yo=(y-1>0?y-1:0); yo<(y+2<SHEET_SIZE?y+2:SHEET_SIZE); yo++)
        if (active[xo][yo] && ! (xo == x && yo == y))
		{
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            float direction0 = paperPos[x][y][0]-paperPos[xo][yo][0];
            float direction1 = paperPos[x][y][1]-paperPos[xo][yo][1];
            float direction2 = paperPos[x][y][2]-paperPos[xo][yo][2];
            float actualDistance = sqrt(direction0*direction0+direction1*direction1+direction2*direction2);
                
            float force = (expectedDistance - actualDistance);
            SETot += force*force;
			SENum += 1;
		}
		
	  if (SETot/SENum > HIGH_STRESS_THRESHHOLD)
	  {
	    // paperPos is in x,y,z order
		int xb = (int)(paperPos[x][y][0]/STRESS_BLOCK_SIZE);
		int yb = (int)(paperPos[x][y][1]/STRESS_BLOCK_SIZE);
		int zb = (int)(paperPos[x][y][2]/STRESS_BLOCK_SIZE);
		
		for(int xo = xb-(xb>0); xo <= xb+(xb+1<VF_SIZE/STRESS_BLOCK_SIZE); xo++)
		for(int yo = yb-(yb>0); yo <= yb+(yb+1<VF_SIZE/STRESS_BLOCK_SIZE); yo++)
		for(int zo = zb-(zb>0); zo <= zb+(zb+1<VF_SIZE/STRESS_BLOCK_SIZE); zo++)
 		{
			highStress[zo][yo][xo] = true;
			r = true;
		}
		
		printf("High stress at x,y,z=%f,%f,%f\n",paperPos[x][y][0],paperPos[x][y][1],paperPos[x][y][2]);
	  }
	}
	
	return r;
}

bool HasHighStress(int x, int y, int z)
{
	return highStress[z/STRESS_BLOCK_SIZE][y/STRESS_BLOCK_SIZE][x/STRESS_BLOCK_SIZE];
}

void Output(const char *filename)
{
  FILE *f = fopen(filename,"w");
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
	  // output in x,y,z order 
      fprintf(f,"%d,%d,%f,%f,%f\n",x,y,paperPos[x][y][0],paperPos[x][y][1],paperPos[x][y][2]); 
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
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            float direction0 = paperPos[x][y][0]-paperPos[xo][yo][0];
            float direction1 = paperPos[x][y][1]-paperPos[xo][yo][1];
            float direction2 = paperPos[x][y][2]-paperPos[xo][yo][2];
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
			AddPointToLookup(point(paperPos[x][y][0],paperPos[x][y][1],paperPos[x][y][2]));
	}
    for(int x = 0; x<SHEET_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE; y++)
    {
      if (TryToFill(x,y,np0,np1,np2))
	  { 
        bool okayToFill = true;
        int px = (int)np0;
		int py = (int)np1;
		int pz = (int)np2;
        if (px>=MARGIN && px<VF_SIZE-MARGIN && py>=MARGIN && py<VF_SIZE-MARGIN && pz>=MARGIN && pz<VF_SIZE-MARGIN)
		{
		  if (HasHighStress(px,py,pz))
			  okayToFill = false;
		  else if (GetDistanceAtPoint(pointInt(px,py,pz))>0)
			  okayToFill = false;
          else if (HasCloseNeighbour(point(np0,np1,np2)))
			  okayToFill = false;
		  if (okayToFill)
		  {
            //printf("New point at %d,%d\n",x,y);  
		    newPtsPaper.push_back(point(x,y,0.0));
            newPts.push_back(point(np0,np1,np2));
		  }
		}
      }
    }
    printf("%d points added\n",(int)newPts.size());     

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

      paperPos[x][y][0] = get<0>(scrollPoint);
      paperPos[x][y][1] = get<1>(scrollPoint);
      paperPos[x][y][2] = get<2>(scrollPoint);

	  paperVel[x][y][0] = 0.0;
	  paperVel[x][y][1] = 0.0;
	  paperVel[x][y][2] = 0.0;
	  
      active[x][y]=true;
	  SetVectorField(x,y);
    }
}

int main(int argc, char *argv[])
{ 
  char outputFileName[256],outputStressFileName[256]; 
  if (argc != 9+4)
  {
	printf("Usage: simpaper5 <seed> <vector field zarr> <surface points output> <stress output>\n");
	printf("Where seed is x,y,z,ax,ay,az,bx,by,bz - coordinates and two vectors specifying orientation of plane\n");
	return -1;
  }

  for(int i=0; i<9; i++)
    seeds[0][i] = atof(argv[i+1]);

  int totPointsAdded = 0;
  
  pointSet newPtsPaper;
  pointSet newPts;

  vectorField = ZARROpen_6(argv[9+1]);
  InitExpectedDistanceLookup();
  
  InitialiseSeed();
    
  for(int i = 0; i<150; i++)
  {
    printf("--- %d ---\n",i);
    int j = 0;
    while (j<10 || (Forces()>0.001 && j<100))
	{
      j++;

      for(int x = 0; x<SHEET_SIZE; x++)
      for(int y = 0; y<SHEET_SIZE; y++)
	  {
		if (active[x][y])
		{
		  bool moved = false;
	      for(int j = 0; j<3; j++)
          {
            paperVel[x][y][j] += paperAcc[x][y][j];
		    int oldPos = (int)paperPos[x][y][j]; 
            paperPos[x][y][j] += paperVel[x][y][j];
		    moved = moved || ((int)paperPos[x][y][j] != oldPos);
		  }
          if (moved)
            SetVectorField(x,y);
        }		
	  }
	}
	
	if (MarkHighStress())
	{
	  printf("High stress encountered\n");
	  break;
	}
	
    newPts.clear();
    newPtsPaper.clear();
	totPointsAdded += MakeNewPoints(newPts,newPtsPaper);
	AddNewPoints(newPts,newPtsPaper);
	
    if (totPointsAdded <10 && i==10)
	{
		printf("Aborting, too few points added");
		break;
	}

  }
  sprintf(outputFileName,argv[9+2]);
  sprintf(outputStressFileName,argv[9+3]);
  
  Output(outputFileName);
  StressOutput(outputStressFileName);
  
  ZARRClose_6(vectorField);
  
  
  return 0;
}

