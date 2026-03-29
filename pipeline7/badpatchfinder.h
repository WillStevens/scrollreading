#pragma once

#include <string> 
#include <vector>
#include <unordered_map>
#include <set>
#include <list>

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#include "parameters.h"
#include "common_types.h"

#define COLLECT_DISTANCE_DISTRIB
#undef OUTPUT_DISTANCE_TIF

#define ROUND(x) (((x)-(int)(x))<0.5?(int)(x):((int)(x))+1)

// what resolution to use for points in rendered?
#define RESCALE 1

#define R_ARRAY_SIZE 1000


#define MAKE_KEY(x,y) ((((uint64_t)x)<<32)+(uint64_t)y)

class BadPatchFinder
{
	public:
		BadPatchFinder()
		{
			ClearRendered();
			maxDistance = 0.0;
		}

		void ClearRendered(void);
		void PlacePatch(const Patch &p1, int patchNum, const affineTx &aftx);

#ifdef OUTPUT_DISTANCE_TIF
		void RenderDistances(void);
#endif

		void CollectDistanceStats(void);

		void FindBadPatches(const AlignmentMap &am, std::map<int,Patch> *patches, std::set<int> &badPatches);
	
	private:
		int rendered_i[R_ARRAY_SIZE][R_ARRAY_SIZE];
		float rendered_f[R_ARRAY_SIZE][R_ARRAY_SIZE][3];

		float maxDistance;
		
#ifdef COLLECT_DISTANCE_DISTRIB

#define D_SIZE_X 2000
#define D_SIZE_Y 2000
#define D_OFF_X 1000
#define D_OFF_Y 1000
		float distances[D_SIZE_X][D_SIZE_Y];

#endif
};

