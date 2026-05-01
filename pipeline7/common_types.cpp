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

// TODO - cache the begin and end positions
// TODO - would be better to rename Begin to Init, because it isn't the beginning, it just initialises the iterator so that the beginning will be found next
PatchIterator Patch::Begin()
{
	PatchIterator pi;
	
	pi.x = 0; pi.y = -1;
	
	pi.p = NULL;
		
	return pi;
}

bool Patch::Next(PatchIterator &pi)
{
	while(true)
	{
		pi.y++;
		
		if (pi.y>maxuy-minuy)
		{
			pi.y=0;
			pi.x++;
			
			if (pi.x>maxux-minux)
				return false;
		}
		
		if (pointGrid[pi.x][pi.y])
		{
			pi.p = pointGrid[pi.x][pi.y];
			return true;
		}
	}
}

void Patch::Flip(void)
{
	if (interpolatedPointGrid)
	{
		fprintf(stderr,"Error: called flip after interpolation\n");
		exit(-1);
	}
	
	for(int x=0; x<(maxux-minux+1)/2; x++)
	{
		for(int y = 0; y<maxuy-minuy+1; y++)
		{
			patchPoint *tmp;
			tmp = pointGrid[x][y];
			pointGrid[x][y] = pointGrid[maxux-minux-x][y];
			pointGrid[maxux-minux-x][y] = tmp;
			
			if (pointGrid[x][y]) pointGrid[x][y]->x = x;
			if (pointGrid[maxux-minux-x][y]) pointGrid[maxux-minux-x][y]->x = maxux-minux-x;
		}
	}
}

bool Patch::Write(const std::string &path, int i)
{
	std::stringstream fileName;
	
	fileName << path << "/patch_" << i << ".bin";
	
	FILE *fo = fopen(fileName.str().c_str(),"w");

	printf("Writing patches\n");
	if(fo)
	{
		gridPointStruct p;
	
		for(int x = minux; x<=maxux; x++)
		for(int y = minuy; y<=maxuy; y++)
		{
			if (pointGrid[x-minux][y-minuy])
			{
				p.x = pointGrid[x-minux][y-minuy]->x;
				p.y = pointGrid[x-minux][y-minuy]->y;
				p.px = pointGrid[x-minux][y-minuy]->v.x;
				p.py = pointGrid[x-minux][y-minuy]->v.y;
				p.pz = pointGrid[x-minux][y-minuy]->v.z;
				
				fwrite(&p,sizeof(p),1,fo);
			}
		}
		
		fclose(fo);
		return true;
	}
	else
		return false;

}

void Patch::BuildFromPoints(vector<patchPoint> &points)
{
	CalcExtents(points);
	MakeGrid(points);
	// Estimate the radius using minx,maxx,miny,maxy
	// Currently radius is only used for visualisation in patchsprings, so it doesn't matter too much if it's wrong.
	radius = abs(minux)>maxux ? abs(minux) : maxux;
	if (maxuy>radius || abs(minuy>radius))
		radius = abs(minuy)>maxuy ? abs(minuy) : maxuy;
}

bool Patch::Read(const std::string &path, int i)
{
    vector<patchPoint> points;
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
		
		BuildFromPoints(points);
		patchNum = i;
		
		return true;
	}
	else
		return false;
}

void Patch::CalcExtents(vector<patchPoint> &points)
{
	if (points.size()==0)
	{
		fprintf(stderr,"Error: Patch::CalcExtents called when points is empty\n");
		exit(-1);
	}
	
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
	return minz;
}

int Patch::MaxZ(void)
{
	return maxz;
}

bool Patch::ContainsZ(int z)
{
	return z>=minz && z<=maxz;
}

void Patch::Interpolate(void)
{
	if (interpolatedPointGrid != NULL)
		return;
	
	interpolatedPointGrid=(patchPoint***)malloc(sizeof(patchPoint**)*(maxux-minux+1)*QUADMESH_SIZE);
	
	for(int i = minux*QUADMESH_SIZE; i<=maxux*QUADMESH_SIZE; i++)
	{
		interpolatedPointGrid[i-minux*QUADMESH_SIZE]=(patchPoint**)malloc(sizeof(patchPoint*)*(maxuy-minuy+1)*QUADMESH_SIZE);
		memset(interpolatedPointGrid[i-minux*QUADMESH_SIZE],0,sizeof(patchPoint*)*(maxuy-minuy+1)*QUADMESH_SIZE);
	}
	
	Vec3 lower,upper,o;
  
	for(int x = 0; x<(maxux-minux+1)*QUADMESH_SIZE; x++)
	for(int y = 0; y<(maxuy-minuy+1)*QUADMESH_SIZE; y++)
	{
		int xs = x/QUADMESH_SIZE;
		int ys = y/QUADMESH_SIZE;
		int xm = x%QUADMESH_SIZE;
		int ym = y%QUADMESH_SIZE;
	  
		// deal with the case where all 4 corners are present
		if (xs+1<maxux-minux-1 && ys+1<maxuy-minuy+1)
		{
			if (pointGrid[xs][ys] && pointGrid[xs+1][ys] && pointGrid[xs][ys+1] && pointGrid[xs+1][ys+1])
			{
				// bilinear interpolation
			
				lower = (pointGrid[xs][ys]->v * (QUADMESH_SIZE-xm) + pointGrid[xs+1][ys]->v * xm)/QUADMESH_SIZE;

				upper = (pointGrid[xs][ys+1]->v * (QUADMESH_SIZE-xm) + pointGrid[xs+1][ys+1]->v * xm)/QUADMESH_SIZE;
			
				o = (lower*(QUADMESH_SIZE-ym)+upper*ym)/QUADMESH_SIZE;
			
				interpolatedPointGrid[x][y] = new patchPoint(x,y,o.x,o.y,o.z);
			}
		}
	}
}

std::vector<patchPoint> Patch::InterpolateAtZ(int zcoord)
{
	std::vector<patchPoint> r;
  	
	Vec3 lower,upper,o;
  
	for(int x = 0; x<(maxux-minux+1)*QUADMESH_SIZE; x++)
	for(int y = 0; y<(maxuy-minuy+1)*QUADMESH_SIZE; y++)
	{
		int xs = x/QUADMESH_SIZE;
		int ys = y/QUADMESH_SIZE;
		int xm = x%QUADMESH_SIZE;
		int ym = y%QUADMESH_SIZE;
	  
		// deal with the case where all 4 corners are present
		if (xs+1<maxux-minux-1 && ys+1<maxuy-minuy+1)
		{
			if (pointGrid[xs][ys] && pointGrid[xs+1][ys] && pointGrid[xs][ys+1] && pointGrid[xs+1][ys+1])
			{
				// bilinear interpolation
			
				lower = (pointGrid[xs][ys]->v * (QUADMESH_SIZE-xm) + pointGrid[xs+1][ys]->v * xm)/QUADMESH_SIZE;

				upper = (pointGrid[xs][ys+1]->v * (QUADMESH_SIZE-xm) + pointGrid[xs+1][ys+1]->v * xm)/QUADMESH_SIZE;
			
				o = (lower*(QUADMESH_SIZE-ym)+upper*ym)/QUADMESH_SIZE;

				if ((int)o.z==zcoord)
					r.push_back(patchPoint(x,y,o.x,o.y,o.z));
				
			}
		}
	}
	
	return r;
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

			// Weight depends on xd,yd and radius : centre = 1.0, boundary = 0.0, linear ramp in between
			// TODO - Could improve on this - it doesn't take account of patch holes, nor the fact that in an octagon the
			// vertices are at radius distance from centre, but points along the edges aren't. This means that there will
			// be step changes in weighting. Better to have a smooth function.
			weight = 1-sqrt(xd*xd+yd*yd)/radius;
			if (weight<0.0) weight=0.0;
			else if (weight>1.0) weight=1.0;
			
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

bool Patch::GetNormal(int x, int y, Vec3 &v)
{
	// Deal with a few cases:
	// 1. if neighbours in 4 directions, calculate normal
	// 2. if neighbours in 3 directions, calculate normal
	// 3. if neighbours in 2 orthogonal directions, calculate normal
	// 4. fail
	
	if (x>=minux && x<=maxux && y>=minuy && y<=maxuy)
	{
		x -= minux;
		y -= minuy;
	}
	else
		return false;
	
	if (pointGrid[x][y])
	{
		if (x>0 && y>0 && x<maxux-minux && y<maxuy-minuy && pointGrid[x-1][y] && pointGrid[x+1][y] && pointGrid[x][y-1] && pointGrid[x][y+1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x-1][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y-1]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (y>0 && x<maxux-minux && y<maxuy-minuy && pointGrid[x+1][y] && pointGrid[x][y-1] && pointGrid[x][y+1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y-1]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (x>0 && y>0 && y<maxuy-minuy && pointGrid[x-1][y] && pointGrid[x][y-1] && pointGrid[x][y+1])
		{
			Vec3 v1 = pointGrid[x][y]->v - pointGrid[x-1][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y-1]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (x>0 && x<maxux-minux && y<maxuy-minuy && pointGrid[x-1][y] && pointGrid[x+1][y] && pointGrid[x][y+1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x-1][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (x>0 && y>0 && x<maxux-minux && pointGrid[x-1][y] && pointGrid[x+1][y] && pointGrid[x][y-1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x-1][y]->v;
			Vec3 v2 = pointGrid[x][y]->v - pointGrid[x][y-1]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (x<maxux-minux && y<maxuy-minuy && pointGrid[x+1][y] && pointGrid[x][y+1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (x>0 && y<maxuy-minuy && pointGrid[x-1][y] && pointGrid[x][y+1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (y>0 && x<maxux-minux && pointGrid[x+1][y] && pointGrid[x][y-1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else if (x>0 && y>0 && pointGrid[x-1][y] && pointGrid[x][y-1])
		{
			Vec3 v1 = pointGrid[x+1][y]->v - pointGrid[x][y]->v;
			Vec3 v2 = pointGrid[x][y+1]->v - pointGrid[x][y]->v;
			v = Vec3::cross(v1,v2).normalized();
		}
		else
		{
			return false;
		}

		return true;
	}
	else
	{
		return false;
	}
}

// TODO - in future the grid representation will be the only one used.
// both are used side by side for now until older code is rewritten
void Patch::MakeGrid(std::vector<patchPoint> &points)
{	
	pointGrid = (patchPoint***)malloc(sizeof(patchPoint**)*(maxux-minux+1));
	
	for(int i = minux; i<=maxux; i++)
	{
		pointGrid[i-minux]=(patchPoint**)malloc(sizeof(patchPoint*)*(maxuy-minuy+1));
		memset(pointGrid[i-minux],0,sizeof(patchPoint*)*(maxuy-minuy+1));
	}
	
	for(auto &pt : points)
	{
		pointGrid[(int)pt.x-minux][(int)pt.y-minuy] = new patchPoint(pt);
	}
}

void Patch::DestroyGrid(void)
{
	if (pointGrid==NULL)
		return;
		
	for(int x = minux; x<=maxux; x++)
	{
		for(int y = minuy; y<=maxuy; y++)
			free(pointGrid[x-minux][y-minuy]);
			
		free(pointGrid[x-minux]);
	}

	free(pointGrid);
	pointGrid=NULL;
}

void Patch::DestroyInterpolatedGrid(void)
{
	if (interpolatedPointGrid==NULL)
		return;
		
	for(int x = minux*QUADMESH_SIZE; x<=maxux*QUADMESH_SIZE; x++)
	{
		for(int y = minuy*QUADMESH_SIZE; y<=maxuy*QUADMESH_SIZE; y++)
			free(interpolatedPointGrid[x-minux][y-minuy]);
		
		free(interpolatedPointGrid[x-minux]);
	}

	free(interpolatedPointGrid);
	interpolatedPointGrid=NULL;
}
