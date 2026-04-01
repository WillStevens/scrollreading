#include <sstream>
#include <string>

#include "common_types.h"

int dirVectorLookup[4][2] = { {1,0},{0,1},{-1,0},{0,-1}};

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

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
		if (point.v.x<minx || first)
			minx = point.v.x;
		if (point.v.z>maxx || first)
			maxx = point.v.x;
		if (point.v.y<miny || first)
			miny = point.v.y;
		if (point.v.y>maxy || first)
			maxz = point.v.y;
		if (point.v.z<minz || first)
			minz = point.v.z;
		if (point.v.z>maxz || first)
			maxz = point.v.z;
		first = false;
	}
}

void Patch::Interpolate(void)
{
	CalcExtents();
	interpolatedPoints = new vector<patchPoints>();
	
	// Now do the interpolation....
}

void Patch::DiscardInterpolation(void)
{
	if (interpolatedPoints)
		delete interpolatedPoints;
}
