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

using namespace std;

#define VF_SIZE 508
#define SHEET_SIZE 509

#define SPRING_FORCE_CONSTANT 0.025f
#define BEND_FORCE_CONSTANT 0.025f
#define FRICTION_CONSTANT 0.9f
#define VECTORFIELD_CONSTANT 0.05f

#define STEP_SIZE 3

typedef tuple<float,float,float> point;
typedef tuple<int,int,int> pointInt;
typedef vector<point> pointSet;

map< pointInt, pointSet > pointLookup;

#define NEIGHBOUR_RADIUS 2
#define NEIGHBOUR_RADIUS_FLOAT 2.0

int dirVectorLookup[4][2] = { {1,0},{0,1},{-1,0},{0,-1}};

float vectorField[VF_SIZE][VF_SIZE][VF_SIZE][3];
float distanceToNearest[VF_SIZE][VF_SIZE][VF_SIZE];

bool active[SHEET_SIZE][SHEET_SIZE];

float paperPos[SHEET_SIZE][SHEET_SIZE][3];
float paperVel[SHEET_SIZE][SHEET_SIZE][3];
float paperAcc[SHEET_SIZE][SHEET_SIZE][3];

float expectedDistanceLookup[3][3];

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

// Initialize the coordinates of the first active point
void InitialiseSeed(void)
{
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2][0] = 256;
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2][1] = 328;
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2][2] = 256;

  paperPos[SHEET_SIZE/2][SHEET_SIZE/2+1][0] = 256+STEP_SIZE;
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2+1][1] = 328;
  paperPos[SHEET_SIZE/2][SHEET_SIZE/2+1][2] = 256;

  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2][0] = 256;
  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2][1] = 328;
  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2][2] = 256+STEP_SIZE;

  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2+1][0] = 256+STEP_SIZE;
  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2+1][1] = 328;
  paperPos[SHEET_SIZE/2+1][SHEET_SIZE/2+1][2] = 256+STEP_SIZE;

  active[SHEET_SIZE/2][SHEET_SIZE/2] = true;
  active[SHEET_SIZE/2][SHEET_SIZE/2+1] = true;
  active[SHEET_SIZE/2+1][SHEET_SIZE/2] = true;
  active[SHEET_SIZE/2+1][SHEET_SIZE/2+1] = true;
}

void LoadVectorField(const char *fname)
{
  FILE *f=fopen(fname,"r");
  
  float vx,vy,vz,dist;


  for(int z=0; z<VF_SIZE; z++)
  for(int y=0; y<VF_SIZE; y++)
  for(int x=0; x<VF_SIZE; x++)
  {
    fscanf(f,"%f,%f,%f,%f",&vx,&vy,&vz,&dist);
	vectorField[z][y][x][0] = vx;
	vectorField[z][y][x][1] = vy;
	vectorField[z][y][x][2] = vz;
    distanceToNearest[z][y][x] = dist;
  }
  
  fclose(f);
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
  for(int x=0; x<SHEET_SIZE; x++)
  for(int y=0; y<SHEET_SIZE; y++)
  {
	paperAcc[x][y][0] = 0.0;
	paperAcc[x][y][1] = 0.0;
	paperAcc[x][y][2] = 0.0;
  }
  
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
          paperAcc[x][y][0] += vectorField[pz][py][px][0]*VECTORFIELD_CONSTANT;
          paperAcc[x][y][1] += vectorField[pz][py][px][1]*VECTORFIELD_CONSTANT;
          paperAcc[x][y][2] += vectorField[pz][py][px][2]*VECTORFIELD_CONSTANT;
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
	  
      // If xo2,yo2 is in bound, then no need to check xo1,yo1
      if (xo2>=0 && xo2<SHEET_SIZE && yo2>=0 && yo2<SHEET_SIZE && active[xo1][yo1] && active[xo2][yo2])
	  {
        rp0 = 2*paperPos[xo1][yo1][0] - paperPos[xo2][yo2][0];
        rp1 = 2*paperPos[xo1][yo1][1] - paperPos[xo2][yo2][1];
        rp2 = 2*paperPos[xo1][yo1][2] - paperPos[xo2][yo2][2];
		return true;
	  }
      int xa1 = xp+yd;
	  int ya1 = yp+xd;
      int xc1 = xp+xd+yd;
	  int yc1 = yp+xd+yd;
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
		  return true;
	  }
	}
  }
  return false;
}

void Output(const char *filename)
{
  FILE *f = fopen(filename,"w");
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
      fprintf(f,"%f,%f,%f\n",paperPos[x][y][0],paperPos[x][y][1],paperPos[x][y][2]);
    else
      fprintf(f,"0,0,0\n");		
  fclose(f);
}

void StressOutput(const char *filename)
{
  FILE *f = fopen(filename,"w");
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
	{
	  float stressTot = 0;
	  int stressNum = 0;
      for(int xo=(x-1>0?x-1:0); xo<(x+2<SHEET_SIZE?x+2:SHEET_SIZE); xo++)
      for(int yo=(y-1>0?y-1:0); yo<(y+2<SHEET_SIZE?y+2:SHEET_SIZE); yo++)
        if (active[xo][yo] && ! (xo == x && yo == y))
		{
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            float direction0 = paperPos[x][y][0]-paperPos[xo][yo][0];
            float direction1 = paperPos[x][y][1]-paperPos[xo][yo][1];
            float direction2 = paperPos[x][y][2]-paperPos[xo][yo][2];
            float actualDistance = sqrt(direction0*direction0+direction1*direction1+direction2*direction2);
                
            float force = (expectedDistance - actualDistance)*SPRING_FORCE_CONSTANT;
            stressTot += fabs(force);
			stressNum += 1;
		}
		
		fprintf(f,"%f\n",stressTot/stressNum);
	}
    else
      fprintf(f,"0\n");		
  fclose(f);
}

int main(void)
{  
  float np0,np1,np2;
  pointSet newPtsPaper;
  pointSet newPts;

  LoadVectorField("vectorfield_test_smooth_508.csv");
  InitExpectedDistanceLookup();
  InitialiseSeed();
  
  for(int i = 0; i<100; i++)
  {
    printf("---\n");
    int j = 0;
    while (j<10 || (Forces()>0.001 && j<100))
	{
      j++;

      for(int x = 0; x<SHEET_SIZE; x++)
      for(int y = 0; y<SHEET_SIZE; y++)
	  for(int j = 0; j<3; j++)
      {
        paperVel[x][y][j] += paperAcc[x][y][j];
        paperPos[x][y][j] += paperVel[x][y][j];
	  }
	}
    newPts.clear();
    newPtsPaper.clear();
	ClearPointLookup();
    for(int x = 0; x<SHEET_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE; y++)
    {
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
        if (px>=0 && px<VF_SIZE && py>=0 && py<VF_SIZE && pz>=0 && pz<VF_SIZE)
		{
          float dist = distanceToNearest[pz][py][px];
		  
		  if (dist>0)
			  okayToFill = false;
          if (HasCloseNeighbour(point(np0,np1,np2)))
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
 
    for(int i = 0; i<(int)newPts.size(); i++)
    {
	  point paperPoint = newPtsPaper[i];
	  int x = get<0>(paperPoint);
	  int y = get<1>(paperPoint);
	  point scrollPoint = newPts[i];
      paperPos[x][y][0] = get<0>(scrollPoint);
      paperPos[x][y][1] = get<1>(scrollPoint);
      paperPos[x][y][2] = get<2>(scrollPoint);
      active[x][y]=true;
    }
  }
  
  Output("testout_508c.csv");
  StressOutput("testout_508c_stress.csv");
  
  return 0;
}

