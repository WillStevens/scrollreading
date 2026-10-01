#pragma once

#include <string> 
#include <vector>
#include <unordered_map>
#include <set>
#include <list>
#include <tuple>

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "parameters.h"
#include "common_types.h"

// COLLECT_DISTANCE_DISTRIB filled distances[2000][2000]: 16 MB cleared for every pair and for
// every chain, and written to for every point. Nothing ever reads it back. CollectDistanceStats
// has no callers, and its body ends in two comments without computing anything. Turning it off
// changes no output because no output depended on it.
#undef COLLECT_DISTANCE_DISTRIB
#undef OUTPUT_DISTANCE_TIF

// ROUND is asymmetric about zero: for a negative x, (int)x truncates towards zero, so
// x-(int)x is negative, always below 0.5, and the result is the truncation rather than the
// nearest integer. Everything in (k-1,k] lands on k for k<0, while [k-0.5,k+0.5) lands on k
// for k>0: the 2D grid of the negative half plane is offset by half a cell (2 voxels here)
// from that of the positive half plane. Build with -DSYMMETRIC_ROUND to round to nearest on
// both sides; the default is left as it was so that published numbers still reproduce.
#ifdef SYMMETRIC_ROUND
#define ROUND(x) ((int)floorf((x)+0.5f))
#else
#define ROUND(x) (((x)-(int)(x))<0.5?(int)(x):((int)(x))+1)
#endif

// what resolution to use for points in rendered?
#define RESCALE 1

// Must be large enough for 2*(max radius)*(1+max expected patch sequence length)
#define R_ARRAY_SIZE 1000

#define MAKE_KEY(x,y) ((((uint64_t)x)<<32)+(uint64_t)y)

// The state of rendering one pair or one chain: the grid the first patch is placed into, the list
// of cells it dirtied, and the three accumulators. These used to be members, that is shared. Split
// out like this, each thread can hold its own and the loops over pairs and over chains become
// parallel without changing any result.
struct RenderState
{
	std::vector<float> grid;   // R_ARRAY_SIZE*R_ARRAY_SIZE cells of 6 floats
	std::vector<int> touched;
	float maxDistance;
	long cellsOverlap, cellsBlind;
	long offGrid;          // points that fell outside the grid and were therefore skipped

	RenderState() : grid((size_t)R_ARRAY_SIZE*R_ARRAY_SIZE*6, 0.0f),
	                maxDistance(0.0f), cellsOverlap(0), cellsBlind(0), offGrid(0) {}

	// Clearing only the cells that were written is the difference between 170 KB and 24 MB of
	// memset per pair: a patch touches a few thousand cells out of a million.
	void Clear()
	{
		for(int i : touched)
		{
			float *c = &grid[(size_t)i*6];
			c[0]=c[1]=c[2]=c[3]=c[4]=c[5]=0.0f;
		}
		touched.clear();
		maxDistance = 0.0f;
		cellsOverlap = 0;
		cellsBlind = 0;
		offGrid = 0;
	}
};

class BadPatchFinder
{
	public:
		BadPatchFinder()
		{
			ClearRendered();
			maxDistance = 0.0;
		}

		void ClearRendered(void);
		void PlacePatch(Patch &p1, int patchNum, const affineTx &aftx, bool first);
		void PlacePatchInto(RenderState &st, Patch &p1, int patchNum, const affineTx &aftx, bool first);

		// Normals do not depend on the transform a patch is placed with, so they are the same
		// every time that patch appears in a chain. With chains up to length five the same patch
		// comes back hundreds of times. Computed once up front, the cache is read-only afterwards
		// and the parallel loops need no lock on it.
		void PrecomputeNormals(std::map<int,Patch> *patches);

#ifdef OUTPUT_DISTANCE_TIF
		void RenderDistances(void);
#endif

#ifdef COLLECT_DISTANCE_DISTRIB
		void CollectDistanceStats(void);
#endif

		// Per-pair instrumentation, written out as pairstats.csv by mode c. It changes no
		// existing output; it only records what the distance test had to work with.
		// cellsBlind counts overlapping cells where neither patch has a normal: there the
		// projected distance is zero whatever the real 3D separation is.
		long cellsOverlap, cellsBlind;
		// (a, b, chain length, maxDistance, overlapping cells, cells with no normal)
		std::vector<std::tuple<int,int,int,float,long,long>> pairStats;


		void FindBadPatches(const AlignmentMap &am, std::map<int,Patch> *patches, std::set<int> &badPatches, std::vector<std::tuple<int,int,float>> &badPatchScores);
	
		void FindBadPatchesGeneral(AlignmentMap &am, std::map<int,Patch> *patches, int length, std::set<int> &badPatches, std::vector<std::tuple<int,int,float>> &badPatchScores);

		void FindNeighbourProblems(std::map<int,std::set<int> > &neighbourList, std::map<int,Patch> *patches, std::set<int> badBridges, std::map<int,int> &newBadBridges, std::vector<int> &patchOrder, std::map<int,affineTx> &patchPositions);

	private:
		// Render state for the serial paths. The parallel loops use one per thread.
		RenderState mainState;

		std::unordered_map<int, std::vector<Vec3>> normals;

		float maxDistance;
		
#ifdef COLLECT_DISTANCE_DISTRIB

#define D_SIZE_X 2000
#define D_SIZE_Y 2000
#define D_OFF_X 1000
#define D_OFF_Y 1000
		float distances[D_SIZE_X][D_SIZE_Y];

#endif
};

