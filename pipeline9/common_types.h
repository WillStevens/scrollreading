#pragma once

#include <unordered_map>
#include <utility>
#include <functional>
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

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

typedef vector<point> pointSet;

typedef struct {
	point pos,vel,vectorField;
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
		bool FindGlobalXY(std::tuple<float,float,float,float,float> patchPosition,float x, float y, Vec3 &v, Vec3 &normal, float &weight);
		void TransformPoint(std::tuple<float,float,float,float,float>,float x, float y, float &xo, float &yo);
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

		// Read-only, safe to call from multiple threads as long as no thread
		// is concurrently building/flipping/clearing this patch.
		const std::vector<patchPoint *>& Points() const { return flat; }
	public:
		void BuildFlat();
		std::vector<patchPoint *> flat;
		
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

typedef std::tuple<int,float,float,float,float,float,float,float,float,float,float,float,float,bool> alignment;
typedef std::map<int,std::vector<alignment> > AlignmentMap;

typedef std::tuple<float,float,float,float,float,float> affineTx;

struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const {
        std::size_t h1 = std::hash<int>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};


typedef std::unordered_map<std::pair<int,int>, float, PairHash> AlignmentScorePairMap;
typedef std::unordered_map<int, float> AlignmentScoreMap;

bool ends_with(std::string const &value, std::string const &ending);

affineTx AffineTxMultiply(const affineTx &m, const affineTx &n);
affineTx AffineTxInverse(const affineTx &aftx);
void AffineTxApply(const affineTx &a, float &x, float &y);
void AffineTxToXYA(const affineTx &aftx, float &x, float &y, float &angle);

float Distance(float x0, float y0, float z0, float x1, float y1, float z1);
float Distance(float x0, float y0, float x1, float y1);
float DotProduct(float x0, float y0, float x1, float y1);

void PatchNumberToColour(int i, int &r, int &g, int &b);
