#pragma once

#include <cstdio>
#include <string>

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
		VectorFieldCalculator(const std::string &zarrName);
		~VectorFieldCalculator();
		
		void GetVectorField(int x, int y, int z, Vec3 &v);
        void GetSmoothedVectorField(int x, int y, int z, Vec3 &v);
		

		ZARR_1 *surfaceZarr;
		float sortedDistances[SORTED_DIST_SIZE][7];

};

