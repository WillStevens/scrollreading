#pragma once

// uint32_t below. libstdc++ stopped pulling <cstdint> in transitively, so GCC 13 and
// later stop here without it.
#include <cstdint>

#include <string>
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

// The hash of a cell of the point lookup and of the distance lookup.
//
// It was h1 ^ (h2<<1) ^ (h3<<2) with h1, h2, h3 the std::hash of an int, which in libstdc++ is the
// identity: three coordinates of about eleven bits each, exclusive-ored one bit apart, land in a
// narrow band of values that is anything but uniform for points that lie on a surface. Counted on
// a patch holding 1,432 cells: 342 of 2,357 buckets carried anything at all, the longest chain was
// 15, and a lookup examined 5.84 elements on average against the 1.3 a good hash gives at that
// load, with the figure rising as the patch grew. That is a lookup whose cost grows with the
// number of points, in a routine called about seven hundred thousand times per run, thirty-six
// buckets at a time.
//
// Multiply, exclusive-or, shift, with odd constants and a final avalanche. Which bucket a key
// lands in is not observable: the map is never walked in full, each key owns its own vector, and
// the order inside that vector is the order the points were added in and not the order of the
// hash.
struct pointIntHash {
		size_t operator()(const pointInt& p) const {
			uint64_t h = (uint64_t)(uint32_t)p.x * 0x9E3779B97F4A7C15ull;
			h ^= (uint64_t)(uint32_t)p.y * 0xC2B2AE3D27D4EB4Full;
			h ^= (uint64_t)(uint32_t)p.z * 0x165667B19E3779F9ull;
			h ^= h >> 29;
			h *= 0xBF58476D1CE4E5B9ull;
			h ^= h >> 32;
			return (size_t)h;
		}
};


// The output folder used to be a compile-time constant, so pointing the chain at a different run
// meant recompiling all of it. Worse, three objects (anneal.o, visitorder.o, omissiontest.o) do
// not depend on parameters.h, so after a change they kept the PREVIOUS folder compiled in and
// wrote there in silence, with no error anywhere.
// It is now read once from SIMPAPER_OUTPUT_DIR, falling back to the compiled value, so one binary
// serves any number of runs and the identity of the old behaviour is preserved when unset.
const char *outputDir();
std::string outPath(const char *suffix);

typedef vector<point> pointSet;

// Holds the position ONLY. Velocity and vector field live in separate arrays inside
// PatchGenerator: the hot loop reads the eight neighbours and needs nothing but .pos from them,
// so keeping them together pulled in 36 bytes of cache line for 12 useful bytes.
// Measured 2026-09-11: that loop is bound by memory bandwidth, not by the CPU.
typedef struct {
	point pos;
} paperPoint;

struct patchPoint {
	float x, y;
	point v;
	
    constexpr patchPoint() : x(0), y(0) {}
    constexpr patchPoint(float x_, float y_, float vx_, float vy_, float vz_) : x(x_), y(y_), v(vx_,vy_,vz_) {}
	constexpr patchPoint(const patchPoint &p) : x(p.x),y(p.y),v(p.v) {}
};

extern int dirVectorLookup[4][2];

class PatchIterator
{
	public:
		PatchIterator() : x(-1),y(-1),p(NULL) {}
		int x,y;
		patchPoint *p;
};

class Patch
{
	public:
		bool Empty(void) {return pointGrid==NULL;}
		void Flip(void);
		bool Write(const std::string &path, int i);
		void BuildFromPoints(std::vector<patchPoint> &points, int patchNum);
		void BuildFromPoints(std::vector<patchPoint> &points,std::vector<std::tuple<int,int,int>> &colours, int patchNum);
		bool Read(const std::string &path, int i);

		PatchIterator Begin();
		bool Next(PatchIterator &pi);
		
		void CalcExtents(std::vector<patchPoint> &points);
		void Interpolate(void);
		vector<patchPoint> InterpolateAtZ(int zcoord);
		void DiscardInterpolation(void);

		int MinX(void); 
		int MaxX(void);
		int MinY(void); 
		int MaxY(void);
		int MinZ(void); 
		int MaxZ(void);
		bool ContainsZ(int z);
		
		void SetPosition(float x, float y, float a) {xpos=x;ypos=y;angle=a;positionSet=true;}
		void UnsetPosition(void) {positionSet=false;}
		bool PatchXYToGlobalXY(float x, float y, float &gx, float &gy);
		bool FindGlobalXY(std::tuple<float,float,float> patchPosition,float x, float y, Vec3 &v, Vec3 &normal, float &weight);
		void TransformPoint(std::tuple<float,float,float>,float x, float y, float &xo, float &yo);
		bool GetNormal(int x, int y, Vec3 &v);
		bool CentreVolCoords(Vec3 &v);
		void CreateParallelPatch(float distance, Patch &target);

		void MakeGrid(std::vector<patchPoint> &points, int patchNum);
		void MakeColourGrid(std::vector<patchPoint> &points, std::vector<std::tuple<int,int,int>> &colours);
		void DestroyGrid(void);
		void DestroyColourGrid(void);
		void DestroyInterpolatedGrid(void);	
	
		void SetPatchNum(int n) {patchNum = n;}
		int GetPatchNum(void) {return patchNum;}
		
		Patch(void) {interpolatedPointGrid = NULL; minux=maxux=minuy=maxuy=minx=maxx=miny=maxy=minz=maxz=-1; positionSet=false; pointGrid=NULL;colourGrid=NULL;}
		
		~Patch(void) {DestroyInterpolatedGrid();DestroyGrid();DestroyColourGrid();}
		void Clear(void) {DestroyInterpolatedGrid();DestroyGrid();interpolatedPointGrid = NULL; minux=maxux=minuy=maxuy=minx=maxx=miny=maxy=minz=maxz=-1; positionSet=false; pointGrid=NULL;colourGrid=NULL;}

	public:
		//vector<patchPoint> *interpolatedPoints;
		int minux,maxux,minuy,maxuy;
		int minx,maxx,miny,maxy,minz,maxz;
		int radius;
		
		bool positionSet;
		float xpos,ypos,angle;
		int patchNum;
		
		patchPoint ***pointGrid;
		uint32_t **colourGrid;
		patchPoint ***interpolatedPointGrid;
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

void PatchNumberToColour(int i, int &r, int &g, int &b);
