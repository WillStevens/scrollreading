#pragma once

#include <vector>

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
    constexpr patchPoint(int x_, int y_, float vx_, float vy_, float vz_) : x(x_), y(y_), v(vx_,vy_,vz_) {}

};

extern int dirVectorLookup[4][2];

class Patch
{
	public:		
	    vector<patchPoint> points;
};

typedef std::tuple<float,float,float,float,float,float> affineTx;

affineTx AffineTxMultiply(const affineTx &m, const affineTx &n);

float Distance(float x0, float y0, float z0, float x1, float y1, float z1);
float Distance(float x0, float y0, float x1, float y1);
float DotProduct(float x0, float y0, float x1, float y1);
