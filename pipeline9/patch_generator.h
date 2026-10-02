#pragma once

#include <unordered_map>
#include <string>

using namespace std;

#include "zarr_1.h"

#include "parameters.h"
#include "common_types.h"
#include "calcvectorfield.h"

#define SHEET_SIZE (MAX_GROWTH_STEPS+5)

class ForcePool;

class PatchGenerator
{
	public:
		// The pool calls these two directly; they were public already, and the pool is a friend
		// of nothing: it holds a pointer to the generator and calls the same two methods the
		// serial loop calls.
		ForcePool *pool = nullptr;

        PatchGenerator(const string &surfaceZarrName_);
		~PatchGenerator(void);

        void MakeActive(int x, int y);		
		bool SetSeed(const std::vector<float> &seed);
	    void ClearPointLookup(void);
        void AddPointToLookup(const point &p);
		float GetDistanceAtPoint(int xp, int yp, int zp);
		bool HasCloseNeighbour(const point &p);
		void SetVectorField(int x, int y);
		void InitExpectedDistanceLookup(void);
		// The two loop bodies, pulled out so that there is one copy of each instead of two kept
		// in step by hand.
		void ForcesOnPoint(int ai, float &maxForce);
		// The same body for up to 16 consecutive entries of activeList at once, one AVX-512 lane
		// per point, each lane doing ForcesOnPoint's operations in ForcesOnPoint's order. The
		// running maximum is folded lane by lane in ai order with the same std::max. Called only
		// when the CPU has AVX-512F; ForcesOnPoints below picks.
		void ForcesOnPoint16(int ai0, int n, float &maxForce);
		void ForcesOnPoints(int lo, int hi, float &maxForce);
		void MovePoint(int ai);
		float ForcesAndMove(void);
		// The relaxation loop. Returns the number of iterations performed, like the loop it
		// replaces.
		int RelaxToConvergence(float &finalForce);
		bool TryToFill(int xp, int yp, point &rp);
		void ClearHighStress(void);
		bool MarkHighStress(void);
		bool HasHighStress(int x, int y, int z);
		int MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper);
		void AddNewPoints(pointSet &newPts, pointSet &newPtsPaper);

		void OutputBoundary(Patch &boundary, pointSet &boundaryPoints, pointSet &boundaryPointsPaper);
		void OutputPatch(Patch &patch, int iter);

        int GeneratePatch(const std::vector<float> &seed,Patch &patch, Patch &boundary, int iter, bool _silent=false);
	private:
		string surfaceZarrName;
	    ZARR_1 *surfaceZarr;

		VectorFieldCalculator *vfc;
		
        // Stored as x=0, y=1, z=2
        paperPoint paperSheet[SHEET_SIZE][SHEET_SIZE];     // .pos only, 12 bytes per point
        // Velocity and vector field, taken out of paperPoint so the hot loop does not waste
        // bandwidth: of the eight neighbours it reads the position and nothing else.
        point velSheet[SHEET_SIZE][SHEET_SIZE];
        point vfSheet[SHEET_SIZE][SHEET_SIZE];
				
        int activeList[SHEET_SIZE*SHEET_SIZE][2];
        int activeListSize;
        bool active[SHEET_SIZE][SHEET_SIZE];

        // Used by ForcesAndMove: the second loop updates the positions, but SetVectorField
        // writes to vectorFieldLookup, which is shared and not thread-safe. Points that changed
        // voxel are marked here and SetVectorField is called for them in a serial pass, in the
        // original order of ai, so the result stays identical.
        bool vfToRedo[SHEET_SIZE*SHEET_SIZE];
		
		unordered_map< pointInt, pointSet, pointIntHash > pointLookup;
		unordered_map< pointInt, float, pointIntHash> distanceLookup;

		// Same reason as the tap cache in calcvectorfield: a flat open-addressed table instead
		// of unordered_map, so each lookup is one cache line rather than a bucket read followed
		// by a pointer chase.
		VectorFieldCalculator::FieldCache vectorFieldLookup;

		// If stress exceeds a threshhold, then mark the volume
		// Don't need full resolution, so this shouldn't take up too much space
		// e.g. if VF_SIZE is 2048 and STRESS_BLOCK_SIZE is 16 then this array takes up 2Mb
		//#define HIGH_STRESS_THRESHHOLD 0.08
		//#define HIGH_STRESS_THRESHHOLD 0.8
		#define STRESS_BLOCK_SIZE 16

		// The stress table used to be a full array sized on the whole scroll:
		// bool[VOL_SIZE_Z/16][VOL_SIZE_Y/16][VOL_SIZE_X/16], which on an 18977x6844x6844 volume
		// is 206 MiB PER INSTANCE, and there is one instance per thread.
		// Measured 2026-09-11: 620 MB of the RSS were the three extra arrays of the three extra
		// threads. Of those 206 MiB one patch touches a few tens of kilobytes.
		//
		// Here the same information lives in pages allocated on demand. Memory becomes
		// proportional to what is actually touched, and the code stays valid for any volume,
		// with no constant tied to a particular scroll or a particular box.
		// Rounded up, not down. No scroll of the twenty three has a side that is a multiple
		// of 16, so truncating leaves the last block along each axis with no cell: on this
		// volume z 20960 to 20973, and y and x 6608 to 6620, fall outside the table.
		// Upstream indexes that block anyway and writes past the end of a member array, which
		// corrupts whatever follows it in the object rather than faulting. The paged table
		// this series uses refuses the index instead, so nothing is corrupted, but the stress
		// of that outer shell is silently dropped and the growth there is guided by a table
		// that is always false. Rounding up covers the volume and moves no existing index.
		#define STRESS_NX ((VOL_SIZE_X+STRESS_BLOCK_SIZE-1)/STRESS_BLOCK_SIZE)
		#define STRESS_NY ((VOL_SIZE_Y+STRESS_BLOCK_SIZE-1)/STRESS_BLOCK_SIZE)
		#define STRESS_NZ ((VOL_SIZE_Z+STRESS_BLOCK_SIZE-1)/STRESS_BLOCK_SIZE)
		#define STRESS_PAGE 32                      // blocks per side: 32^3 = 32 KiB per page
		#define STRESS_PX ((STRESS_NX+STRESS_PAGE-1)/STRESS_PAGE)
		#define STRESS_PY ((STRESS_NY+STRESS_PAGE-1)/STRESS_PAGE)
		#define STRESS_PZ ((STRESS_NZ+STRESS_PAGE-1)/STRESS_PAGE)
		unsigned char *stressPage[STRESS_PZ][STRESS_PY][STRESS_PX];
		unsigned char *StressPage(int zb, int yb, int xb, bool creating);
		
		float expectedDistanceLookup[3][3];

		bool silent;
};