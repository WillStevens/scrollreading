#pragma once

#include "parameters.h"
#include "common_types.h"

#define SHEET_SIZE (MAX_GROWTH_STEPS+5)

class PatchGenerator
{
	public:
	    PatchGenerator(std::string surfaceZarrName);		
		~PatchGenerator(void);

        void MakeActive(int x, int y);		
		bool SetSeed(float seed[9]);
	    void ClearPointLookup(void);
        void AddPointToLookup(const point &p);
		float GetDistanceAtPoint(const pointInt &p);
		bool HasCloseNeighbour(const point &p);
		void SetVectorField(int x, int y);
		void InitExpectedDistanceLookup(void);
		float ForcesAndMove(void);
		bool TryToFill(int xp, int yp, float &rp0, float &rp1, float &rp2);
		bool MarkHighStress(void);
		bool HasHighStress(int x, int y, int z);
		int MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper);
		void AddNewPoints(pointSet &newPts, pointSet &newPtsPaper);

		int GeneratePatch(void);
	private:
	    ZARR_1 *sufaceZarr;
		ZARR_c64i1b256 *vectorFieldZarr;

        // Stored as x=0, y=1, z=2
        paperPoint paperSheet[SHEET_SIZE][SHEET_SIZE];
				
        int activeList[SHEET_SIZE*SHEET_SIZE][2];
        int activeListSize;
        bool active[SHEET_SIZE][SHEET_SIZE];
		
		map< pointInt, pointSet > pointLookup;
		map< pointInt, float> distanceLookup;

		unordered_map< uint64_t, point> vectorFieldLookup;

		// If stress exceeds a threshhold, then mark the volume
		// Don't need full resolution, so this shouldn't take up too much space
		// e.g. if VF_SIZE is 2048 and STRESS_BLOCK_SIZE is 16 then this array takes up 2Mb
		//#define HIGH_STRESS_THRESHHOLD 0.08
		//#define HIGH_STRESS_THRESHHOLD 0.8
		#define STRESS_BLOCK_SIZE 16
		bool highStress[VOL_SIZE_Z/STRESS_BLOCK_SIZE][VOL_SIZE_Y/STRESS_BLOCK_SIZE][VOL_SIZE_X/STRESS_BLOCK_SIZE];
		
		float expectedDistanceLookup[3][3];

};