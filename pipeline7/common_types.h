#pragma once

#include <vector>

using namespace std;

#include "vec3.h"

typedef Vec3 point;
typedef int pointInt[3];

typedef vector<point> pointSet;

typedef struct {
	point pos,vel,vectorField;
} paperPoint;

typedef struct{
	int x, y;
	point v;
} patchPoint;

float PointDistance(const point &p, const point &q);
float DotProduct(const point &p, const point &q);

extern int dirVectorLookup[4][2];

class Patch
{
	public:
	    vector<patchPoint> points;
};
