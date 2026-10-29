#pragma once

#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <array>

using namespace std;

#include "zarr_1.h"

#include "parameters.h"
#include "common_types.h"
#include "calcvectorfield.h"

//#define SHEET_SIZE (MAX_GROWTH_STEPS+5)
//#define SHEET_SIZE 600

class PatchGenerator
{
	public:
        PatchGenerator(const string &surfaceZarrName_, size_t sheet_size_ = (MAX_GROWTH_STEPS+5));
		~PatchGenerator(void);

        void MakeActive(int x, int y);		
		bool SetSeed(const std::vector<float> &seed);
	    void ClearPointLookup(void);
        void AddPointToLookup(const point &p);
		float GetDistanceAtPoint(int xp, int yp, int zp);
		bool HasCloseNeighbour(const point &p);
		void SetVectorField(int x, int y);
		void InitExpectedDistanceLookup(void);
		float ForcesAndMove(float fieldWeight=1.0);
		bool TryToFill(int xp, int yp, point &rp);
		void ClearHighStress(void);
		bool MarkHighStress(void);
		bool HasHighStress(int x, int y, int z);
		int MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper, bool ignoreSurfacePred=false);
		void AddNewPoints(pointSet &newPts, pointSet &newPtsPaper);

		void OutputBoundary(Patch &boundary, pointSet &boundaryPoints, pointSet &boundaryPointsPaper);
		void OutputPatch(Patch &patch, int iter);

        int GeneratePatch(const std::vector<float> &seed,Patch &patch, Patch &boundary, int iter, bool _silent=false);
		
		void FillHoles(const std::string &fileName,const std::string &outputFileName);

		void Dump(std::ostream &os);
	private:
	    int SHEET_SIZE;   // runtime sheet dimension (sheet is SHEET_SIZE x SHEET_SIZE)
		string surfaceZarrName;
	    ZARR_1 *surfaceZarr;

		VectorFieldCalculator *vfc;
		
        // Flat, row-major storage (index = x*SHEET_SIZE + y). Use sheet(x,y) / act(x,y)
        // Stored as x=0, y=1, z=2
        std::vector<paperPoint> paperSheet;

        std::vector< std::array<int,2> > activeList;   // activeList[i][0]=x, [i][1]=y
        int activeListSize;
        std::vector<unsigned char> active;             // 0/1 flags (not vector<bool>, for speed)

        size_t sheetIndex(int x, int y) const { return (size_t)x * (size_t)SHEET_SIZE + (size_t)y; }
        paperPoint &sheet(int x, int y)       { return paperSheet[sheetIndex(x,y)]; }
        unsigned char &act(int x, int y)      { return active[sheetIndex(x,y)]; }

		unordered_map< pointInt, pointSet, pointIntHash > pointLookup;
		unordered_map< pointInt, float, pointIntHash> distanceLookup;

		unordered_map< uint64_t, point> vectorFieldLookup;

		// If stress exceeds a threshhold, then mark the volume
		// Don't need full resolution, so this shouldn't take up too much space
		// e.g. if VF_SIZE is 2048 and STRESS_BLOCK_SIZE is 16 then this array takes up 2Mb
		//#define HIGH_STRESS_THRESHHOLD 0.08
		//#define HIGH_STRESS_THRESHHOLD 0.8
		#define STRESS_BLOCK_SIZE 16
		bool highStress[VOL_SIZE_Z/STRESS_BLOCK_SIZE][VOL_SIZE_Y/STRESS_BLOCK_SIZE][VOL_SIZE_X/STRESS_BLOCK_SIZE];
		
		float expectedDistanceLookup[3][3];

		bool silent;
};