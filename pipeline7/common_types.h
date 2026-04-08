#pragma once

#include <vector>
#include <map>

using namespace std;

#include "vec3.h"

typedef Vec3 point;
class pointInt
{
  public:
	bool operator==(const pointInt& other) const
	{
		return x==other.x && y==other.y && z==other.z;
	}
	
	int x,y,z;
};

struct pointIntHash {
		size_t operator()(const pointInt& p) const {
			size_t h1 = hash<int>{}(p.x);
			size_t h2 = hash<int>{}(p.y);
			size_t h3 = hash<int>{}(p.z);
			
			return h1^(h2<<1)^(h3<<2);
		}
};


typedef vector<point> pointSet;

typedef struct {
	point pos,vel,vectorField;
} paperPoint;

struct patchPoint {
	float x, y;
	point v;
	
    constexpr patchPoint() : x(0), y(0) {}
    constexpr patchPoint(float x_, float y_, float vx_, float vy_, float vz_) : x(x_), y(y_), v(vx_,vy_,vz_) {}

};

extern int dirVectorLookup[4][2];

class Patch
{
	public:		
		void Flip(void);
		bool Write(const std::string &path, int i);
		bool Read(const std::string &path, int i);
		
		void CalcExtents(bool force);
		void Interpolate(void);
		void DiscardInterpolation(void);

		int MinZ(void); 
		int MaxZ(void);
		bool ContainsZ(int z);
		
		Patch(void) {interpolatedPoints = NULL; minx=maxx=miny=maxy=minz=maxz=-1;}
		~Patch(void) {DiscardInterpolation();}
		
	    vector<patchPoint> points;
		vector<patchPoint> *interpolatedPoints;
		int minx,maxx,miny,maxy,minz,maxz;
};

typedef std::tuple<int,float,float,float,float,float,float,float,float,float,float,float,float> alignment;
typedef std::map<int,std::vector<alignment> > AlignmentMap;

typedef std::tuple<float,float,float,float,float,float> affineTx;

bool ends_with(std::string const &value, std::string const &ending);

affineTx AffineTxMultiply(const affineTx &m, const affineTx &n);
affineTx AffineTxInverse(const affineTx &aftx);
void AffineTxApply(const affineTx &a, float &x, float &y);
void AffineTxToXYA(const affineTx &aftx, float &x, float &y, float &angle);

float Distance(float x0, float y0, float z0, float x1, float y1, float z1);
float Distance(float x0, float y0, float x1, float y1);
float DotProduct(float x0, float y0, float x1, float y1);
