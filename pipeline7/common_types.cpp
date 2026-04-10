#include <sstream>
#include <string>
#include <cmath>
#include <cstring>

#include "common_types.h"

#include "parameters.h"

int dirVectorLookup[4][2] = { {1,0},{0,1},{-1,0},{0,-1}};

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size())
    { return false; }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

affineTx AffineTxMultiply(const affineTx &m, const affineTx &n)
{
	/* a,b,c,d,e,f are the coeffs of the transform
	   (Yx) = ( a b c ) (Xx)
	   (Yy)   ( d e f ) (Xy)
	   (1)    ( 0 0 1 ) (1)
	*/
	
	float a = std::get<0>(m);
	float b = std::get<1>(m);
	float c = std::get<2>(m);
	float d = std::get<3>(m);
	float e = std::get<4>(m);
	float f = std::get<5>(m);

	float ad = std::get<0>(n);
	float bd = std::get<1>(n);
	float cd = std::get<2>(n);
	float dd = std::get<3>(n);
	float ed = std::get<4>(n);
	float fd = std::get<5>(n);
	
	return affineTx(a*ad+b*dd,a*bd+b*ed,a*cd+b*fd+c,d*ad+e*dd,d*bd+e*ed,d*cd+e*fd+f);
}

void AffineTxApply(const affineTx &aftx, float &x, float &y)
{
	float x_ = x, y_ = y;
	float a = std::get<0>(aftx);
	float b = std::get<1>(aftx);
	float c = std::get<2>(aftx);
	float d = std::get<3>(aftx);
	float e = std::get<4>(aftx);
	float f = std::get<5>(aftx);

	x = a*x_+b*y_+c;
    y = d*x_+e*y_+f;		
}

affineTx AffineTxInverse(const affineTx &aftx)
{
	// For this nice inversion formula see
	// https://negativeprobability.blogspot.com/2011/11/affine-transformations-and-their.html
	float a = std::get<0>(aftx);
	float b = -std::get<1>(aftx);
	float d = -std::get<3>(aftx);
	float e = std::get<4>(aftx);

	float c = -std::get<2>(aftx)*std::get<0>(aftx)+std::get<5>(aftx)*std::get<1>(aftx);
	float f = -std::get<5>(aftx)*std::get<0>(aftx)-std::get<2>(aftx)*std::get<1>(aftx);
	
	return affineTx(a,b,c,d,e,f);
}

void AffineTxToXYA(const affineTx &aftx, float &x, float &y, float &angle)
{
    angle = std::atan2(std::get<3>(aftx),std::get<0>(aftx));
	x = std::get<2>(aftx);
	y = std::get<5>(aftx);
}

float Distance(float x0, float y0, float z0, float x1, float y1, float z1)
{
	float dx = x1-x0, dy = y1-y0, dz = z1-z0;
	
	float d2 = dx*dx+dy*dy+dz*dz;
	
	return sqrt(d2);
}

float Distance(float x0, float y0, float x1, float y1)
{
	return Distance(x0,y0,0,x1,y1,0);
}

float DotProduct(float x0, float y0, float x1, float y1)
{
	return x0*x1+y0*y1;
}

void Patch::Flip(void)
{
	for(patchPoint &pp : points)
	{
		pp.x = -pp.x;
	}
}

bool Patch::Write(const std::string &path, int i)
{
	std::stringstream fileName;
	
	fileName << path << "/patch_" << i << ".bin";
	
	FILE *fo = fopen(fileName.str().c_str(),"w");

	if(fo)
	{
		gridPointStruct p;
	
		for(auto &point : points)
		{
			p.x = point.x;
			p.y = point.y;
			p.px = point.v.x;
			p.py = point.v.y;
			p.pz = point.v.z;
				
            fwrite(&p,sizeof(p),1,fo);	
		}
		
		fclose(fo);
		return true;
	}
	else
		return false;

}

bool Patch::Read(const std::string &path, int i)
{
	points.clear();
	std::stringstream fileName;
	
	fileName << path << "/patch_" << i << ".bin";
	
	FILE *fi = fopen(fileName.str().c_str(),"r");

	if(fi)
	{
		gridPointStruct p;
		fseek(fi,0,SEEK_END);
		long fsize = ftell(fi);
		fseek(fi,0,SEEK_SET);
  
		while(ftell(fi)<fsize)
		{
			fread(&p,sizeof(p),1,fi);
			
			points.push_back(patchPoint(p.x,p.y,p.px,p.py,p.pz));			
		}
		
		fclose(fi);
		
		CalcExtents(true);
		// Estimate the radius using minx,maxx,miny,maxy
		// Currently radius is only used for visualisation in patchsprings, so it doesn't matter too much if it's wrong.
		radius = abs(minux)>maxux ? abs(minux) : maxux;
		if (maxuy>radius || abs(minuy>radius))
			radius = abs(minuy)>maxuy ? abs(minuy) : maxuy;

		return true;
	}
	else
		return false;
}

void Patch::CalcExtents(bool force)
{
	if (!force && minx != -1)
		return;
	
	bool first=true;
	
	for(auto &point : points)
	{
		if (point.x<minux || first)
			minux = point.x;
		if (point.x>maxux || first)
			maxux = point.x;
		if (point.y<minuy || first)
			minuy = point.y;
		if (point.y>maxuy || first)
			maxuy = point.y;

		if (point.v.x<minx || first)
			minx = point.v.x;
		if (point.v.z>maxx || first)
			maxx = point.v.x;
		if (point.v.y<miny || first)
			miny = point.v.y;
		if (point.v.y>maxy || first)
			maxy = point.v.y;
		if (point.v.z<minz || first)
			minz = point.v.z;
		if (point.v.z>maxz || first)
			maxz = point.v.z;
		first = false;
	}
}

int Patch::MinZ(void)
{
	CalcExtents(false);
	return minz;
}

int Patch::MaxZ(void)
{
	CalcExtents(false);
	return maxz;
}

bool Patch::ContainsZ(int z)
{
	CalcExtents(false);
	return z>=minz && z<=maxz;
}

void Patch::Interpolate(void)
{
	if (interpolatedPoints != NULL)
		return;
	
	interpolatedPoints = new vector<patchPoint>();
	
#define SHEET_SIZE (MAX_GROWTH_STEPS+5)

	float paperPos[SHEET_SIZE][SHEET_SIZE][3];
	bool active[SHEET_SIZE][SHEET_SIZE];

	for(int x = 0; x<SHEET_SIZE; x++)
	for(int y = 0; y<SHEET_SIZE; y++)
	  active[x][y] = false;

	for(patchPoint &p : points)
	{
		paperPos[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2][0]=p.v.x;
		paperPos[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2][1]=p.v.y;
		paperPos[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2][2]=p.v.z;
		active[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2] = true;
    }
	
	float lower[3],upper[3],ox,oy,oz;
  
	for(int x = 0; x<SHEET_SIZE*QUADMESH_SIZE; x++)
	for(int y = 0; y<SHEET_SIZE*QUADMESH_SIZE; y++)
	{
		int xs = x/QUADMESH_SIZE;
		int ys = y/QUADMESH_SIZE;
		int xm = x%QUADMESH_SIZE;
		int ym = y%QUADMESH_SIZE;
	  
		// deal with the case where all 4 corners are present
		if (xs+1<SHEET_SIZE && ys+1<SHEET_SIZE)
		{
			if (active[xs][ys] && active[xs+1][ys] && active[xs][ys+1] && active[xs+1][ys+1])
			{
				// bilinear interpolation
			
				lower[0] = (paperPos[xs][ys][0] * (QUADMESH_SIZE-xm) + paperPos[xs+1][ys][0] * xm)/QUADMESH_SIZE;
				lower[1] = (paperPos[xs][ys][1] * (QUADMESH_SIZE-xm) + paperPos[xs+1][ys][1] * xm)/QUADMESH_SIZE;
				lower[2] = (paperPos[xs][ys][2] * (QUADMESH_SIZE-xm) + paperPos[xs+1][ys][2] * xm)/QUADMESH_SIZE;

				upper[0] = (paperPos[xs][ys+1][0] * (QUADMESH_SIZE-xm) + paperPos[xs+1][ys+1][0] * xm)/QUADMESH_SIZE;
				upper[1] = (paperPos[xs][ys+1][1] * (QUADMESH_SIZE-xm) + paperPos[xs+1][ys+1][1] * xm)/QUADMESH_SIZE;
				upper[2] = (paperPos[xs][ys+1][2] * (QUADMESH_SIZE-xm) + paperPos[xs+1][ys+1][2] * xm)/QUADMESH_SIZE;
			
				ox = (lower[0]*(QUADMESH_SIZE-ym)+upper[0]*ym)/QUADMESH_SIZE;
				oy = (lower[1]*(QUADMESH_SIZE-ym)+upper[1]*ym)/QUADMESH_SIZE;
				oz = (lower[2]*(QUADMESH_SIZE-ym)+upper[2]*ym)/QUADMESH_SIZE;
			
				interpolatedPoints->push_back(patchPoint(x,y,ox,oy,oz));
			}
		}
	}
}

void Patch::DiscardInterpolation(void)
{
	if (interpolatedPoints)
	{
		delete interpolatedPoints;
		interpolatedPoints = NULL;
	}
}

/*
bool Patch::FindGlobalXY(float x, float y, Vec3 &v, float weight)
{
	if (positionSet)
	{
		// Inverse transform x,y using xpos,ypos,angle

		float xd = (x-xpos)*cos(angle)+(y-ypos)*sin(angle);
		float yd = -(x-xpos)*sin(angle)+(y-ypos)*cos(angle);
		
		//printf("xd,yd = %f,%f\n",xd,yd);
		
		float p1x = (int)xd, p1y = (int)yd;
		if (xd<0) p1x -= 1;
		if (yd<0) p1y -= 1;
		
		Vec3 corners[4];
		int cornerCount = 0;
		
		for(auto &pt : points)
		{
			if (pt.x==p1x && pt.y==p1y)
			{
				corners[0] = pt.v;
				cornerCount++;
			}
			else if (pt.x==p1x+1 && pt.y==p1y)
			{
				corners[1] = pt.v;
				cornerCount++;
			}
			else if (pt.x==p1x && pt.y==p1y+1)
			{
				corners[2] = pt.v;
				cornerCount++;
			}
			else if (pt.x==p1x+1 && pt.y==p1y+1)
			{
				corners[3] = pt.v;
				cornerCount++;
			}
			
			if (cornerCount==4)
				break;
		}
		
		//printf("cornercount %d\n",cornerCount);
		
		if (cornerCount==4)
		{
			// bilinear interpolation to get vx,vy,vz
			float xf = xd-p1x;
			
			Vec3 i0 = corners[0] + (corners[1]-corners[0])*xf;
			Vec3 i1 = corners[2] + (corners[3]-corners[2])*xf;
			
			float yf = yd-p1y;
			
			v = i0 + (i1-i0)*yf;

			weight = 1.0;
			
			return true;
		}
		
		return false;
	}
	
	return false;
}
*/

bool Patch::FindGlobalXY(float x, float y, Vec3 &v, float &weight)
{
	if (positionSet)
	{
		MakeGrid();
		
		// Inverse transform x,y using xpos,ypos,angle

		float xd = (x-xpos)*cos(angle)+(y-ypos)*sin(angle);
		float yd = -(x-xpos)*sin(angle)+(y-ypos)*cos(angle);
		
		//printf("x,y = %f,%f\n",x,y);
		//printf("xd,yd = %f,%f\n",xd,yd);
		//printf("tx,ty = %f,%f\n",xd*cos(angle)+yd*sin(angle)+xpos,yd*cos(angle)-xd*sin(angle)+ypos);
		
		int p1x = (int)xd, p1y = (int)yd;
		if (xd<0) p1x -= 1;
		if (yd<0) p1y -= 1;
		
		Vec3 corners[4];
		int cornerCount = 0;
		
		if (p1x>=minux && p1x<maxux && p1y>=minuy && p1y<maxuy)
		{
			if (pointGrid[p1x-minux][p1y-minuy])
			{
				corners[0]=pointGrid[p1x-minux][p1y-minuy]->v;
				cornerCount++;
			}
			if (pointGrid[p1x-minux+1][p1y-minuy])
			{
				corners[1]=pointGrid[p1x-minux+1][p1y-minuy]->v;
				cornerCount++;
			}
			if (pointGrid[p1x-minux][p1y-minuy+1])
			{
				corners[2]=pointGrid[p1x-minux][p1y-minuy+1]->v;
				cornerCount++;
			}
			if (pointGrid[p1x-minux+1][p1y-minuy+1])
			{
				corners[3]=pointGrid[p1x-minux+1][p1y-minuy+1]->v;
				cornerCount++;
			}
		}
				
		//printf("cornercount %d\n",cornerCount);
		
		if (cornerCount==4)
		{
			// bilinear interpolation to get vx,vy,vz
			float xf = xd-p1x;
			
			Vec3 i0 = corners[0] + (corners[1]-corners[0])*xf;
			Vec3 i1 = corners[2] + (corners[3]-corners[2])*xf;
			
			float yf = yd-p1y;
			
			v = i0 + (i1-i0)*yf;

			weight = 1.0;
			
			return true;
		}
		
		return false;
	}
	
	return false;
}

void Patch::TransformPoint(float x, float y, float &xo, float &yo)
{
	if (positionSet)
	{
		xo = x*cos(angle)-y*sin(angle)+xpos;
		yo = y*cos(angle)+x*sin(angle)+ypos;
	}
	else
	{
		xo = x; yo = y;
	}
}

// TODO - in future the grid representation will be the only one used.
// both are used side by side for now until older code is rewritten
void Patch::MakeGrid(void)
{
	CalcExtents(false);
	
	if (pointGrid!=NULL)
		return;
	
	pointGrid = (patchPoint***)malloc(sizeof(patchPoint**)*(maxux-minux+1));
	
	for(int i = minux; i<=maxux; i++)
	{
		pointGrid[i-minux]=(patchPoint**)malloc(sizeof(patchPoint*)*(maxuy-minuy+1));
		memset(pointGrid[i-minux],0,sizeof(patchPoint*)*(maxuy-minuy+1));
	}
	
	for(auto &pt : points)
	{
		pointGrid[(int)pt.x-minux][(int)pt.y-minuy] = &pt;
	}
}

void Patch::DestroyGrid(void)
{
	if (pointGrid==NULL)
		return;
		
	for(int i = minux; i<=maxux; i++)
	{
		free(pointGrid[i-minux]);
	}

	free(pointGrid);
	pointGrid=NULL;
}
