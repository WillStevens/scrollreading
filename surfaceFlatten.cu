#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <cuda.h>
#include <thrust/sort.h>
#include <thrust/device_ptr.h>

#define NUM_BLOCKS 4096
#define THREADS_PER_BLOCK 128

#define CELL_DIMX 3.0f
#define CELL_DIMY 3.0f
#define CELL_DIMZ 3.0f

#define MAX_NEIGHBOUR_DISTANCE_SQUARED (2.1f)

#define CELL_NUMX 256
#define CELL_NUMY 256
#define CELL_NUMZ 256

#define MAXX (CELL_NUMX*CELL_DIMX)
#define MAXY (CELL_NUMY*CELL_DIMY)
#define MAXZ (CELL_NUMZ*CELL_DIMZ)

#define EPSILON 0.01f

#define MAXXE (MAXX-EPSILON)
#define MAXYE (MAXY-EPSILON)
#define MAXZE (MAXZ-EPSILON)

#define H_CONSTANT 1.5f
#define H_CONSTANT_TIMES_2 (2.0f*H_CONSTANT)
#define H_CONSTANT_TIMES_2_SQUARED (4.0f*H_CONSTANT*H_CONSTANT)
#define W_CONSTANT 15.0f/(16.0f*PI*H_CONSTANT*H_CONSTANT*H_CONSTANT)

#define VISC 1.0f					// This corresponds to MU_N
#define VISC_BASE 1.0f				// This corresponds to MU_X - MU_N
#define P_0 1.0f					// This is RHO_0 times c squared

#define BOUNDARY_THRESH 0.20f		// This corresponds to BETA

#define GRAVITY_CONSTANT -0.00005f	// This corresponds to g

// The variables KAPPA, u, w and q from the paper are stored in a single cuda float4 type, so here they
// are referred to has x,y,z and w respectively.

#define RHO_0 1.0f

#define ATTRACT_FORCE_CONSTANT 0.1f	// This corresponds to A

#define BOUNDARY_VISC_CONSTANT 10.0f	// This corresponds to T

#define PI 3.14159265358979323846264f

__device__ float LengthVector(float3 a)
{
	return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);
}

__device__ float LengthVector(float4 a)
{
	return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);
}

__device__ float3 MultiplyVector(float3 a, float f)
{
	return make_float3(a.x*f,a.y*f,a.z*f);
}

__device__ float WCalcGivenS(float s)
{
	if (s>=2.0f)
	{
		return 0.0f;
	}
	else
	{
		return W_CONSTANT * (0.25f*s*s-s+1.0f);
	}
}

__device__ float WCalc(float3 rij,float dist)
{
	float s = dist/H_CONSTANT;

	if (s>=2.0f)
	{
		return 0.0f;
	}
	else
	{
		return W_CONSTANT * (0.25f*s*s-s+1.0f);
	}
}

__device__ float3 DeltaWCalc(float3 rij,float dist)
{
	float s = dist/H_CONSTANT;

	if (s>=2.0f)
	{
		return make_float3(0.0f,0.0f,0.0f);
	}
	else
	{
		return MultiplyVector(rij,W_CONSTANT*(0.5f*s-1.0f)/(H_CONSTANT*dist));
	}
}

// This is the part of DeltaWCalc that multiplies the vector
__device__ float FabCalc(float dist)
{
	float s = dist/H_CONSTANT;

	if (s>=2.0f)
	{
		return 0.0f;
	}
	else
	{
		return W_CONSTANT*(0.5f*s-1.0f)/(H_CONSTANT*dist);
	}
}

__device__ float DotProduct(float3 a, float3 b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

// A sigmoid curve that will go from y near 0 to y near 1 as x goes from 0 to 1
__device__ float Sigmoid(float x)
{
	return 1.0f/(1.0f+expf(-12.0f*(x-0.5f)));
}

__device__ float BoundaryViscForce(float y)
{
	return BOUNDARY_VISC_CONSTANT*(0.5f-y)*(0.5f-y);
}

__global__ void InitialiseDevice(float4 *pPos, float4 *pAcc, unsigned *cellHash, unsigned *pIndex, unsigned *trackIndex, unsigned *reverseTrackIndex, float *d_simTime, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		if (zIdx == 0)
		{
			*d_simTime = 0.0f;
		}

		cellHash[zIdx] = 
			(((int)(pPos[zIdx].x/CELL_DIMX))*CELL_NUMX +
			 ((int)(pPos[zIdx].y/CELL_DIMY)))*CELL_NUMY +
		          (int)(pPos[zIdx].z/CELL_DIMZ);
		pIndex[zIdx] = zIdx;
		trackIndex[zIdx] = zIdx;
		reverseTrackIndex[zIdx] = zIdx;
		pAcc[zIdx] = make_float4(0.0f,0.0f,0.0f,0.0f);

		zIdx += stepSize;
	}
}

__global__ void InitCellStart(int *cellStart, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;
	
	for(int i = 0; i<nloops && zIdx < CELL_NUMX*CELL_NUMY*CELL_NUMZ; i++)
	{
		cellStart[zIdx] = -1;

		zIdx += stepSize;
	}
}

__global__ void ArrayCopy(float4 *a0, float4 *a1, float4 *b0, float4 *b1, unsigned *cellHash, int *cellStart, unsigned *pIndex, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	if (zIdx==0 && 0)
	{
	  printf("pIndex...\n");
	  for(int i = 0; i<nParticles; i++)
	  {
	    printf("i=%d, pIndex[i]=%d\n",i,pIndex[i]);
	  }
	}
	
	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		unsigned p = pIndex[zIdx];
		a1[zIdx] = a0[p];
		b1[zIdx] = b0[p];

		if (zIdx == 0 || cellHash[zIdx] != cellHash[zIdx-1])
		{
			cellStart[cellHash[zIdx]] = zIdx;
		}

		zIdx += stepSize;
	}
}

__global__ void UpdateTrackIndex(unsigned *pIndex, unsigned *trackIndex0, unsigned *trackIndex1, unsigned *reverseTrackIndex, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	if (zIdx==0 && 0)
	{
	  printf("pIndex...\n");
	  for(int i = 0; i<nParticles; i++)
	  {
	    printf("i=%d, pIndex[i]=%d\n",i,pIndex[i]);
	  }
	}
	
	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		unsigned p = pIndex[zIdx];           // The particle that did have index p now has index zIdx
		                                     // So trackIndex0[p] is the original index of the particle that did have index p
		trackIndex1[zIdx] = trackIndex0[p];  // The particle that now has index zIdx originally had index trackIndex0[p]

		reverseTrackIndex[trackIndex0[p]] = zIdx; // reverseTrackIndex looks up current index from original index
		
		zIdx += stepSize;
	}
}

__global__ void ParticleMove(float4 *pPos, float4 *pVel, float4 *pAcc, int *pBoundary, unsigned *cellHash, unsigned *pIndex, float *maxAcc, float *simTime, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	float deltaT = 0.25f*sqrtf(H_CONSTANT/(*maxAcc));

	if (deltaT>0.05f) deltaT = 0.05f;

	if (zIdx == 0)
	{
		*simTime += deltaT;
	}

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		pPos[zIdx].x += (pVel[zIdx].x + pAcc[zIdx].x*deltaT)*deltaT;
		pPos[zIdx].y += (pVel[zIdx].y + pAcc[zIdx].y*deltaT)*deltaT;
		pPos[zIdx].z += (pVel[zIdx].z + pAcc[zIdx].z*deltaT)*deltaT;

		pVel[zIdx].x += pAcc[zIdx].x*deltaT;
		pVel[zIdx].y += pAcc[zIdx].y*deltaT;
		pVel[zIdx].z += pAcc[zIdx].z*deltaT;
		pVel[zIdx].w += pAcc[zIdx].w*deltaT;

		// Apply gravitational force at this point, ready for next iteration
		// pAcc[zIdx].w is deltaDensity - set it to zero here
		pAcc[zIdx] = make_float4(0.0f,GRAVITY_CONSTANT,0.0f,0.0f);

		if (pPos[zIdx].x >= MAXXE) {pPos[zIdx].x = MAXXE; pVel[zIdx].x = 0.0f;}
		if (pPos[zIdx].y >= MAXYE) {pPos[zIdx].y = MAXYE; pVel[zIdx].y = 0.0f;}
		if (pPos[zIdx].z >= MAXZE) {pPos[zIdx].z = MAXZE; pVel[zIdx].z = 0.0f;}
		if (pPos[zIdx].x < 0.0f) {pPos[zIdx].x = 0.0f; pVel[zIdx].x = 0.0f;}
		if (pPos[zIdx].y < 0.0f)
		{
			pPos[zIdx].y = 0.0f;
			pVel[zIdx].y = 0.0f;
		}
		if (pPos[zIdx].y < 0.5f) 
		{
			pVel[zIdx].y *= (1.0f - BoundaryViscForce(pPos[zIdx].y)*deltaT);
			pVel[zIdx].x *= (1.0f - BoundaryViscForce(pPos[zIdx].y)*deltaT);
			pVel[zIdx].z *= (1.0f - BoundaryViscForce(pPos[zIdx].y)*deltaT);
		}

		if (pPos[zIdx].z < 0.0f) {pPos[zIdx].z = 0.0f; pVel[zIdx].z = 0.0f;}

		cellHash[zIdx] = 
			(((int)(pPos[zIdx].x/CELL_DIMX))*CELL_NUMX +
			 ((int)(pPos[zIdx].y/CELL_DIMY)))*CELL_NUMY +
		          (int)(pPos[zIdx].z/CELL_DIMZ);

		pIndex[zIdx] = zIdx;

		zIdx += stepSize;
	}
}

#define CELLNUMAUX(x,y,z) ((x)*CELL_NUMX+(y))*CELL_NUMY+(z)
#define CELLNUM(x,y,z) ((x)<0 || (y)<0 || (z)<0 || (x)>=CELL_NUMX || (y)>=CELL_NUMY || (z)>=CELL_NUMZ)?-1:CELLNUMAUX(x,y,z)

/* Called once to count how many neighbours each particle has */
__global__ void CountNeighbours(float4 *pPos, unsigned *cellHash, int *cellStart, int *neighbourCount, unsigned *pIndex, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	int neighbourCell,ps;
	int cellx,celly,cellz;
	int numNeighbours;
	
	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
	    numNeighbours = 0;
		
		cellx = (int)(pPos[zIdx].x/CELL_DIMX); 
		celly = (int)(pPos[zIdx].y/CELL_DIMY);
		cellz = (int)(pPos[zIdx].z/CELL_DIMZ);

		for(int xo=-1;xo<=1;xo++) for(int yo=-1;yo<=1;yo++) for(int zo=-1;zo<=1;zo++)
		{	
			if ((neighbourCell = CELLNUM(cellx+xo,celly+yo,cellz+zo)) != -1 &&
			    (ps = cellStart[neighbourCell]) != -1)
			{
			
				while(ps < nParticles && cellHash[ps] == neighbourCell)
				{
					if (ps != zIdx)
					{
						float3 diff = make_float3(
					        pPos[ps].x - pPos[zIdx].x,
							pPos[ps].y - pPos[zIdx].y,
							pPos[ps].z - pPos[zIdx].z);
							
						float dist2 = diff.x*diff.x+diff.y*diff.y+diff.z*diff.z;
						if (dist2 < MAX_NEIGHBOUR_DISTANCE_SQUARED)
						{
						  numNeighbours++;
						}
					}
					ps++;
				}
			}
		}

	    neighbourCount[pIndex[zIdx]] = numNeighbours;
		
		zIdx += stepSize;
	}
}

/* This is only called once */
__global__ void InitialiseNeighbours(float4 *pPos, unsigned *cellHash, int *cellStart, int *neighbourCount, uint2 *neighbourList, float *neighbourDistance, unsigned *pIndex, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	int neighbourCell,ps;
	int cellx,celly,cellz;
	int neighbourIndex;

/*	
	if (zIdx==0)
	{
	  printf("pIndex...\n");
	  for(int i = 0; i<nParticles; i++)
	  {
	    printf("pIndex[%d]=%d\n",i,pIndex[i]);
	  }

	  printf("neighbourCount...\n");
	  for(int i = 0; i<nParticles; i++)
	  {
	    printf("neighbourCount[%d]=%d\n",i,neighbourCount[i]);
	  }

    }
*/
	
//	if (zIdx==0) printf("Here...%d\n",pIndex[zIdx]);
	
	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
	    neighbourIndex = 0;
		
		cellx = (int)(pPos[zIdx].x/CELL_DIMX); 
		celly = (int)(pPos[zIdx].y/CELL_DIMY);
		cellz = (int)(pPos[zIdx].z/CELL_DIMZ);

//		if (zIdx==0) printf("cell:%d,%d,%d\n",cellx,celly,cellz);

		for(int xo=-1;xo<=1;xo++) for(int yo=-1;yo<=1;yo++) for(int zo=-1;zo<=1;zo++)
		{	
			if ((neighbourCell = CELLNUM(cellx+xo,celly+yo,cellz+zo)) != -1 &&
			    (ps = cellStart[neighbourCell]) != -1)
			{
			
//		        if (zIdx==0) printf("cellx+xo,celly+yo,cellz+zo:%d,%d,%d\n",cellx+xo,celly+yo,cellz+zo);
				while(ps < nParticles && cellHash[ps] == neighbourCell)
				{
/*
					if (zIdx==0) printf("ps:%d\n",ps);
					if (zIdx==0) printf("pIndex[ps]:%d\n",pIndex[ps]);
					if (zIdx==0) printf("ps xyz:%f,%f,%f\n",pPos[ps].x,pPos[ps].y,pPos[ps].z);
					if (zIdx==0) printf("neighbourCell:%d\n",neighbourCell);
*/
					if (ps != zIdx)
					{
						float3 diff = make_float3(
					        	pPos[ps].x - pPos[zIdx].x,
							pPos[ps].y - pPos[zIdx].y,
							pPos[ps].z - pPos[zIdx].z);

//						if (zIdx==0) printf("xyz:%f,%f,%f\n",pPos[zIdx].x,pPos[zIdx].y,pPos[zIdx].z);
//						if (zIdx==0) printf("diff:%f,%f,%f\n",diff.x,diff.y,diff.z);
							
						float dist2 = diff.x*diff.x+diff.y*diff.y+diff.z*diff.z;
//						if (zIdx==0) printf("dist2:%f\n",dist2);
						if (dist2 < MAX_NEIGHBOUR_DISTANCE_SQUARED)
						{
						  // We want neighbourIndex to be the index that the particle originally have (so that we can use trackIndex)
						  // This is currently stored in pIndex, so we can use that to obtain it using trackIndex
						  int zIdxOrigIndex = pIndex[zIdx];
						  int psOrigIndex = pIndex[ps];
						  int offset = neighbourCount[zIdxOrigIndex];
/*
						  if (zIdx==0)
						  {
						    printf("zIdxOrigIndex=%d,psOrigIndex=%d,offset=%d\n",zIdxOrigIndex,psOrigIndex,offset);
						  }
*/						  
						  neighbourList[neighbourIndex+offset].x = zIdxOrigIndex;
						  neighbourList[neighbourIndex+offset].y = psOrigIndex;
						  neighbourDistance[neighbourIndex+offset] = sqrtf(dist2);
						  neighbourIndex++;
						}
					}
					ps++;
				}
			}
		}
		
		zIdx += stepSize;
	}
}

__global__ void ShepardFilter(float4 *pPos, float4 *pVel, float *pNewDensity, unsigned *cellHash, int *cellStart, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	int neighbourCell,ps;
	int cellx,celly,cellz;

	float numerator;
	float denominator;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		cellx = (int)(pPos[zIdx].x/CELL_DIMX); 
		celly = (int)(pPos[zIdx].y/CELL_DIMY);
		cellz = (int)(pPos[zIdx].z/CELL_DIMZ);

		numerator = W_CONSTANT;
		denominator = W_CONSTANT/pVel[zIdx].w;

		for(int xo=-1;xo<=1;xo++) for(int yo=-1;yo<=1;yo++) for(int zo=-1;zo<=1;zo++)
		{	
			if ((neighbourCell = CELLNUM(cellx+xo,celly+yo,cellz+zo)) != -1 &&
			    (ps = cellStart[neighbourCell]) != -1)
			{
				while(ps < nParticles && cellHash[ps] == neighbourCell)
				{
					if (ps != zIdx)
					{
						float3 diff = make_float3(
					        	pPos[ps].x - pPos[zIdx].x,
							pPos[ps].y - pPos[zIdx].y,
							pPos[ps].z - pPos[zIdx].z);

						float dist2 = diff.x*diff.x+diff.y*diff.y+diff.z*diff.z;
						if (dist2 < H_CONSTANT_TIMES_2_SQUARED)
						{
							float dist = sqrtf(dist2);
							float w = WCalc(diff,dist);

							numerator += w;
							denominator += w/pVel[ps].w;
						}
					}
					ps++;
				}
			}
		}

		pNewDensity[zIdx] = numerator/denominator;

		zIdx += stepSize;
	}
}

__global__ void UpdateDensity(float4 *pVel, float *pNewDensity, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		pVel[zIdx].w = pNewDensity[zIdx];

		zIdx += stepSize;
	}
}

__global__ void ParticleForces(float4 *pPos, float4 *pVel, float4 *pAcc, float *pAccLength, int *pBoundary, unsigned *cellHash, int *cellStart, int nParticles, int nloops, unsigned *trackIndex)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	int neighbourCell,ps;
	int cellx,celly,cellz;
	
	float4 thisAcc;
	float thisViscosity;
	float4 vThis;
	float3 boundaryVector;
	float boundaryDensity;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		cellx = (int)(pPos[zIdx].x/CELL_DIMX); 
		celly = (int)(pPos[zIdx].y/CELL_DIMY);
		cellz = (int)(pPos[zIdx].z/CELL_DIMZ);

		vThis = pVel[zIdx];

		thisAcc = make_float4(0.0f,0.0f,0.0f,0.0f);
		thisViscosity = pPos[zIdx].w;

		boundaryVector = make_float3(0.0f,0.0f,0.0f);
		boundaryDensity = 0.0f;

		for(int xo=-1;xo<=1;xo++) for(int yo=-1;yo<=1;yo++) for(int zo=-1;zo<=1;zo++)
		{	
			if ((neighbourCell = CELLNUM(cellx+xo,celly+yo,cellz+zo)) != -1 &&
			    (ps = cellStart[neighbourCell]) != -1)
			{
				while(ps < nParticles && cellHash[ps] == neighbourCell)
				{
					if (ps != zIdx)
					{
						float3 diff = make_float3(
					        	pPos[zIdx].x - pPos[ps].x,
							pPos[zIdx].y - pPos[ps].y,
							pPos[zIdx].z - pPos[ps].z);

						float dist2 = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
						if (dist2 < H_CONSTANT_TIMES_2_SQUARED)
						{
						    float dOther = pVel[ps].w;
						    float dist = sqrtf(dist2);
						    float3 dw = DeltaWCalc(diff,dist);
						    float fab = FabCalc(dist);
						    float w = WCalc(diff,dist);
						    float3 vdiff = make_float3(
						    	vThis.x - pVel[ps].x,
						    	vThis.y - pVel[ps].y,
						    	vThis.z - pVel[ps].z);

						    // Viscous forces
						    if (1)
						    {
							float a = (thisViscosity+pPos[ps].w)/(vThis.w*dOther*dist2);
							a *= DotProduct(diff,dw);
							thisAcc.x += vdiff.x * a;
							thisAcc.y += vdiff.y * a;
							thisAcc.z += vdiff.z * a;
						    }
						    // Pressure forces
						    if (0)
						    {
							float pterm = P_0 *
							 (1.0f/(RHO_0*vThis.w)-1.0f/(vThis.w*vThis.w)
							+ 1.0f/(RHO_0*dOther)-1.0f/(dOther*dOther));
							thisAcc.x -= dw.x*pterm;
							thisAcc.y -= dw.y*pterm;
							thisAcc.z -= dw.z*pterm;
						    }
						    // Density change
						    if (0)
						    {
							    thisAcc.w += DotProduct(vdiff,dw);
						    }
						    // Boundary vector
						    if (1)
						    {
							    boundaryVector.x += diff.x*w;
							    boundaryVector.y += diff.y*w;
							    boundaryVector.z += diff.z*w;
							    boundaryDensity += w;
						    }						    
						}
					}
					ps++;
				}
			}
		}

		pAcc[zIdx].x += thisAcc.x;
		pAcc[zIdx].y += thisAcc.y;
		pAcc[zIdx].z += thisAcc.z;
		pAcc[zIdx].w += thisAcc.w;

		pBoundary[zIdx] = boundaryDensity < 0.00001f || LengthVector(boundaryVector)/boundaryDensity > BOUNDARY_THRESH;

		pAccLength[zIdx] = LengthVector(pAcc[zIdx]);

		zIdx += stepSize;
	}
}


__global__ void ConnectForces(float4 *pPos, float4 *pVel, float4 *pAcc, float *pAccLength, int *pBoundary, unsigned *cellHash, int *cellStart, uint2 *neighbourList, float *neighbourDistance, int nParticlePairs, int nPPloops, unsigned *reverseTrackIndex, int iters)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nPPloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;
	
	if (zIdx == 0 && iters%1000==0)
	{
	  printf("Coordinates...\n");
	  for(int i = 0; i<49; i++)
	  {
	    printf("originalIndex=%d currentIndex=%d, x=%f, y=%f, z=%f\n",i,reverseTrackIndex[i],
		  pPos[reverseTrackIndex[i]].x,
		  pPos[reverseTrackIndex[i]].y,
		  pPos[reverseTrackIndex[i]].z);
	  }
	}
	
	for(int i = 0; i<nPPloops && zIdx < nParticlePairs; i++)
	{

		int p1,p2;
	    float distTarget;
		
		// reverseTrackIndex gives us the current index of the particle that was originally the one in the neighbourList
		p1 = reverseTrackIndex[neighbourList[zIdx].x];
		p2 = reverseTrackIndex[neighbourList[zIdx].y];
		distTarget = neighbourDistance[zIdx];
				
		
		float3 diff = make_float3(
			        	pPos[p1].x - pPos[p2].x,
						pPos[p1].y - pPos[p2].y,
						pPos[p1].z - pPos[p2].z);

		float dist = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);

		if (iters%1000==0) printf("Original particle pair: %d, %d\n",neighbourList[zIdx].x,neighbourList[zIdx].y);
		if (iters%1000==0) printf("Particle pair: %d, %d, distTarget:%f, dist:%f\n",p1,p2,distTarget,dist);
		
		float distFromTarget = dist - distTarget;

        float f = -distFromTarget*ATTRACT_FORCE_CONSTANT;

		float3 a = MultiplyVector(diff,f/dist);
		
//        if (iters%1000==0) printf("Force on %d is: %f,%f,%f\n",p1,a.x,a.y,a.z);		
		
		pAcc[p1].x += a.x;
		pAcc[p1].y += a.y;
		pAcc[p1].z += a.z;
	
		zIdx += stepSize;
	}
}
int nParticles;
int nParticlePairs;
float4 *d_pVel[2];
float *d_pNewDensity;
float4 *d_pPos[2];
float4 *d_pAcc;
float *d_pAccLength;
int *d_pBoundary;
unsigned *d_cellHash;
int *d_cellStart;
unsigned *d_pIndex;
unsigned *d_trackIndex[2];
unsigned *d_reverseTrackIndex;
float *d_simTime;
int *d_neighbourCount;       // How many neighbours does each particle have? Calculate this before allocating memory for below
uint2 *d_neighbourList;     // List of all pairs of particles p,q that are neighbours 
float *d_neighbourDistance;  // The distances between the neighbours in neighbourList


float4 *h_pVel;
float4 *h_pPos;
float h_simTime;
unsigned *h_trackIndex;
int *h_neighbourCount;
uint2 *h_neighbourList;
float *h_neighbourDistance;

int activeArray;

void Check(cudaError_t e)
{
	if (e != cudaSuccess)
	{
		printf("%s\n",cudaGetErrorString(cudaGetLastError()));
		exit(-1);
	}
}

float RandFloat(float min, float max)
{
	return min + (max-min)*((rand()%10000)/10000.0f);
}

void CopyToDevice(void)
{
	cudaMemcpy(d_pVel[activeArray],h_pVel,sizeof(float4)*nParticles,cudaMemcpyHostToDevice);
	cudaMemcpy(d_pPos[activeArray],h_pPos,sizeof(float4)*nParticles,cudaMemcpyHostToDevice);
}

void CopyFromDevice(void)
{
	cudaMemcpy(h_pVel,d_pVel[activeArray],sizeof(float4)*nParticles,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_pPos,d_pPos[activeArray],sizeof(float4)*nParticles,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_trackIndex,d_trackIndex[activeArray],sizeof(unsigned)*nParticles,cudaMemcpyDeviceToHost);
	cudaMemcpy(&h_simTime,d_simTime,sizeof(float),cudaMemcpyDeviceToHost);
}

void AllocateMemory(void)
{
	for(int i = 0; i<2; i++)
	{
		Check( cudaMalloc((void**)&d_pVel[i],sizeof(float4)*nParticles) );
		Check( cudaMalloc((void**)&d_pPos[i],sizeof(float4)*nParticles) );
		Check( cudaMalloc((void**)&d_trackIndex[i],sizeof(unsigned)*nParticles) );
	}
	Check( cudaMalloc((void**)&d_reverseTrackIndex,sizeof(unsigned)*nParticles) );
	Check( cudaMalloc((void**)&d_pNewDensity,sizeof(float)*nParticles) );
	Check( cudaMalloc((void**)&d_pBoundary,sizeof(int)*nParticles) );
	Check( cudaMalloc((void**)&d_pAcc,sizeof(float4)*nParticles) );
	Check( cudaMalloc((void**)&d_pAccLength,sizeof(float)*nParticles) );
	Check( cudaMalloc((void**)&d_cellHash,sizeof(unsigned)*nParticles) );
	Check( cudaMalloc((void**)&d_cellStart,sizeof(int)*CELL_NUMX*CELL_NUMY*CELL_NUMZ) );
	Check( cudaMalloc((void**)&d_pIndex,sizeof(unsigned)*nParticles) );
	Check( cudaMalloc((void**)&d_simTime,sizeof(float)) );
	Check( cudaMalloc((void**)&d_neighbourCount,sizeof(int)*nParticles) );	
	
	h_pVel = (float4 *)malloc(sizeof(float4)*nParticles);
	h_pPos = (float4 *)malloc(sizeof(float4)*nParticles);
	h_trackIndex = (unsigned *)malloc(sizeof(unsigned)*nParticles);
	h_neighbourCount = (int *)malloc(sizeof(int)*nParticles);
}

int AllocateNeighbourMemory(void)
{
	int numNeighbourPairs = 0;
	
	cudaMemcpy(h_neighbourCount,d_neighbourCount,sizeof(int)*nParticles,cudaMemcpyDeviceToHost);

	// Count how many pairs of neighbours there are in total, and make a cumulative total so that we know the offset into
	// neighbourList and neighbourDistance to user for each particle, when building these
	
//	printf("Cumulative neighbourCount...\n");
	for(int i=0; i<nParticles; i++)
	{
	    numNeighbourPairs += h_neighbourCount[i];
	    h_neighbourCount[i] = numNeighbourPairs-h_neighbourCount[i];
//		printf("neighbourCount[%d]=%d\n",i,numNeighbourPairs);
	}

	cudaMemcpy(d_neighbourCount,h_neighbourCount,sizeof(int)*nParticles,cudaMemcpyHostToDevice);

	Check( cudaMalloc((void**)&d_neighbourList,sizeof(uint2)*numNeighbourPairs) );	
	Check( cudaMalloc((void**)&d_neighbourDistance,sizeof(float)*numNeighbourPairs) );	

	h_neighbourList = (uint2 *)malloc(sizeof(uint2)*numNeighbourPairs);
	h_neighbourDistance = (float *)malloc(sizeof(float)*numNeighbourPairs);

	return numNeighbourPairs;
}	


void FreeMemory(void)
{
	for(int i = 0; i<2; i++)
	{
		cudaFree(d_pVel[i]);
		cudaFree(d_pPos[i]);
		cudaFree(d_trackIndex[i]);
	}
	cudaFree(d_reverseTrackIndex);
	cudaFree(d_pNewDensity);
	cudaFree(d_pBoundary);
	cudaFree(d_pAcc);
	cudaFree(d_pAccLength);
	cudaFree(d_cellHash);
	cudaFree(d_cellStart);
	cudaFree(d_pIndex);
	cudaFree(d_simTime);
	cudaFree(d_neighbourCount);
	cudaFree(d_neighbourList);
	cudaFree(d_neighbourDistance);
	free(h_pVel);
	free(h_pPos);
	free(h_trackIndex);
	free(h_neighbourCount);
	free(h_neighbourList);
	free(h_neighbourDistance);
}

int GetNumParticles(char *fname)
{
    int i = 0;
    FILE *f = fopen(fname,"r");
	
	if(f)
	{
		float x,y,z;
	  
	    while(fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
	    {
			i++;
	    }
	}

	return i;
}

void Initialise(char *fname)
{
    FILE *f = fopen(fname,"r");
	
	if(f)
	{
	    int i = 0;
		float x,y,z;
	  
//	    printf("Loading...\n");
	    while(fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
	    {
			if (i<nParticles)
			{
				h_pVel[i].x = 0.0f;
				h_pVel[i].y = 0.0f;
				h_pVel[i].z = 0.0f;
				h_pVel[i].w = RHO_0;
				h_pPos[i].x = x+128;
				h_pPos[i].y = y;
				h_pPos[i].z = z+128;
				h_pPos[i].w = VISC_BASE;
			}
			i++;
	    }
//		printf("Loaded %d particles\n",i);
	}
}

void Display(void)
{
//	printf(":T{%f}\n",h_simTime);

	for(int i = 0; i<nParticles; i++)
	{
		//printf(":P{%d,%.2f,%.2f,%.2f,%.4g,%.4g,%.4g,%.2f,%.2f,%.2f,%.2f}\n",h_trackIndex[i],h_pPos[i].x,h_pPos[i].y,h_pPos[i].z,h_pVel[i].x,h_pVel[i].y,h_pVel[i].z,0.5f,1.0f,1.0f,1.0f);
		
		printf("%.2f,%.2f,%.2f\n",h_pPos[i].x,h_pPos[i].y,h_pPos[i].z);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
	  printf("Usage: surfaceFlatten <input.csv>\n");
	  exit(1);
	}

	nParticles = GetNumParticles(argv[1]);
	
	activeArray = 0;
	int nthds = THREADS_PER_BLOCK;
	int nblks = NUM_BLOCKS;
	int nloops = 1+(nParticles-1)/(nthds*nblks);
	int nCellLoops = 1+(CELL_NUMX*CELL_NUMY*CELL_NUMZ-1)/(nthds*nblks);

	float time = 0.0f;

	
	cudaSetDevice(0);

	cudaEvent_t start,stop;
	cudaEventCreate(&start);
	cudaEventCreate(&stop);

	AllocateMemory();
	Initialise(argv[1]);

	CopyToDevice();

	InitialiseDevice<<< nblks, nthds >>>(d_pPos[activeArray],d_pAcc,d_cellHash,d_pIndex,d_trackIndex[activeArray],d_reverseTrackIndex,d_simTime,nParticles,nloops);
	thrust::sort_by_key(	thrust::device_ptr<uint>(d_cellHash),
				thrust::device_ptr<uint>(d_cellHash+nParticles),
				thrust::device_ptr<uint>(d_pIndex));
	InitCellStart<<< nblks, nthds >>>(d_cellStart,nCellLoops);
	ArrayCopy<<< nblks, nthds >>>(d_pPos[activeArray],d_pPos[1-activeArray],d_pVel[activeArray],d_pVel[1-activeArray],d_cellHash,d_cellStart,d_pIndex,nParticles,nloops);
	UpdateTrackIndex<<< nblks, nthds >>>(d_pIndex,d_trackIndex[activeArray],d_trackIndex[1-activeArray],d_reverseTrackIndex,nParticles,nloops);
	
	activeArray = 1-activeArray;
 
    CountNeighbours<<< nblks, nthds >>>(d_pPos[activeArray],d_cellHash,d_cellStart,d_neighbourCount, d_pIndex,nParticles, nloops);

	nParticlePairs = AllocateNeighbourMemory();

	int nPPloops = 1+(nParticlePairs-1)/(nthds*nblks);

    InitialiseNeighbours<<< nblks, nthds >>>(d_pPos[activeArray],d_cellHash,d_cellStart,d_neighbourCount, d_neighbourList, d_neighbourDistance, d_pIndex,nParticles, nloops);

	cudaMemcpy(h_neighbourList,d_neighbourList,sizeof(uint2)*nParticlePairs,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_neighbourDistance,d_neighbourDistance,sizeof(float)*nParticlePairs,cudaMemcpyDeviceToHost);

/*
	printf("NeighbourList...\n");
	for(int i = 0; i<nParticlePairs; i++)
	{
	  printf("n1=%d,n2=%d,dist=%f\n",h_neighbourList[i].x,h_neighbourList[i].y,h_neighbourDistance[i]);
	}
*/

	cudaEventRecord(start,0);

	int iters = 15000;

	float *maxAcc;

	for(int i = 0; i<iters;i++)
	{
//	        printf("Iteration %d\n",i);
			
			if (i && i%60 == 0)
			{
				ShepardFilter<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pNewDensity,d_cellHash,d_cellStart,nParticles,nloops);
				UpdateDensity<<< nblks, nthds >>>(d_pVel[activeArray],d_pNewDensity,nParticles,nloops);
			}

			ParticleForces<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pAcc,d_pAccLength,d_pBoundary,d_cellHash,d_cellStart,nParticles,nloops,d_trackIndex[activeArray]);

			ConnectForces<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pAcc,d_pAccLength,d_pBoundary,d_cellHash,d_cellStart,d_neighbourList,d_neighbourDistance,nParticlePairs,nPPloops,d_reverseTrackIndex,i);

			maxAcc = thrust::raw_pointer_cast(
					thrust::max_element(thrust::device_ptr<float>(d_pAccLength),
					thrust::device_ptr<float>(d_pAccLength+nParticles))
					);
			ParticleMove<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pAcc,d_pBoundary,d_cellHash,d_pIndex,maxAcc,d_simTime,nParticles,nloops);
			thrust::sort_by_key(thrust::device_ptr<uint>(d_cellHash),
				    thrust::device_ptr<uint>(d_cellHash+nParticles),
				    thrust::device_ptr<uint>(d_pIndex));
			InitCellStart<<< nblks, nthds >>>(d_cellStart,nCellLoops);
			ArrayCopy<<< nblks, nthds >>>(d_pPos[activeArray],d_pPos[1-activeArray],d_pVel[activeArray],d_pVel[1-activeArray],d_cellHash,d_cellStart,d_pIndex,nParticles,nloops);
			UpdateTrackIndex<<< nblks, nthds >>>(d_pIndex,d_trackIndex[activeArray],d_trackIndex[1-activeArray],d_reverseTrackIndex,nParticles,nloops);

			activeArray = 1-activeArray;
	
	}
		

	CopyFromDevice();
	Display();
	
//    for(int i = 0; i<200; i++)
//	  printf("%d\n",h_neighbourIndex[i]);

	cudaEventRecord(stop,0);
	cudaEventSynchronize(stop);
	cudaEventElapsedTime(&time,start,stop);

	//printf("%f %d\n",time,iters);

	FreeMemory();
	return 0;
}
