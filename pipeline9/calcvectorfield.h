#pragma once

#include <cstdio>
#include <string>
#include <unordered_map>

#include "zarr_1.h"
#include "vec3.h"
#include "parameters.h"

// Calculate a single vector field point

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SMOOTH_WINDOW 2
#define GAUSS_SCALE 1

class VectorFieldCalculator
{
	public:
		VectorFieldCalculator(ZARR_1 *sf);
		~VectorFieldCalculator();
		
		void GetVectorField(int x, int y, int z, Vec3 &v);
        void GetSmoothedVectorField(int x, int y, int z, Vec3 &v);
		void GetSmoothedVectorFieldInt8(int x, int y, int z, Vec3 &v);
		

		ZARR_1 *surfaceZarr;
		float sortedDistances[SORTED_DIST_SIZE][7];

		std::unordered_map< uint64_t, Vec3> vectorFieldLookup;

};

