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
#define THREADS_PER_BLOCK 1024

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

#define XZ_OFFSET 128

#define EPSILON 0.01f

#define MAXXE (MAXX-EPSILON)
#define MAXYE (MAXY-EPSILON)
#define MAXZE (MAXZ-EPSILON)

#define REPEL_FORCE_CONSTANT            0.2f
#define ATTRACT_FORCE_CONSTANT          0.4f
#define FRICTION_FORCE_CONSTANT         0.9f
#define GRAVITY_FORCE_CONSTANT 			0.0f
#define RESTORE_FORCE_CONSTANT 			0.01f

#define PI 3.14159265358979323846264f

#define RESTORE_TIME 10000 // Number of iterations after which target particles will be back to their original positions 
#define RELAX_TIME 20000 // Time allowed for hole particles to relax

#define DEBUG_OUT

__device__ float LengthVector(float3 a)
{
	return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);
}

__device__ float LengthVector(float4 a)
{
	return sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);
}

__device__ float3 MultiplyVector(float3 a,float f)
{
	return make_float3(a.x*f,a.y*f,a.z*f);
}

__device__ float DotProduct(float3 a,float3 b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

__global__ void InitialiseDevice(float4 *pPos, float4 *pAcc, unsigned *cellHash, unsigned *pIndex, unsigned *trackIndex, unsigned *reverseTrackIndex, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
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

__global__ void ParticleMove(float4 *pPos, float4 *pVel, float4 *pAcc, float4 *pOriginalPos, float4 *pTargetPos, unsigned *cellHash, unsigned *trackIndex, unsigned *pIndex, int nParticles, int nloops, int nTargetParticles, int iters)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	float a = RESTORE_TIME-iters;
	float b = iters;
	
	if (a<0) a=0;
	if (b>RESTORE_TIME) b=RESTORE_TIME;
	
/*
	if (zIdx==0)
	{
	  for(int i = 0; i<nParticles; i++)
	  {
        printf("xyz of %d is: %f,%f,%f\n",i,pPos[i].x,pPos[i].y,pPos[i].z);		
        printf("force on %d is: %f,%f,%f\n",i,pAcc[i].x,pAcc[i].y,pAcc[i].z);			    
	  }
	}
*/	
	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		unsigned ti = trackIndex[zIdx];

			if ((ti==189357 || ti==189358) && 0)
			{
				printf("%d: xyz is: %f,%f,%f\n",ti,pPos[zIdx].x,pPos[zIdx].y,pPos[zIdx].z);		
				printf("%d: force is: %f,%f,%f\n",ti,pAcc[zIdx].x,pAcc[zIdx].y,pAcc[zIdx].z);			    
			}

		pVel[zIdx].x += pAcc[zIdx].x;
		pVel[zIdx].y += pAcc[zIdx].y;
		pVel[zIdx].z += pAcc[zIdx].z;
		
    	pVel[zIdx].x *= FRICTION_FORCE_CONSTANT;
		pVel[zIdx].y *= FRICTION_FORCE_CONSTANT;
		pVel[zIdx].z *= FRICTION_FORCE_CONSTANT;

		pPos[zIdx].x += pVel[zIdx].x;
		pPos[zIdx].y += pVel[zIdx].y;
		pPos[zIdx].z += pVel[zIdx].z;

		// Apply gravitational force at this point, ready for next iteration
		// pAcc[zIdx].w is deltaDensity - set it to zero here
		pAcc[zIdx] = make_float4(0.0f,-GRAVITY_FORCE_CONSTANT,0.0f,0.0f);
		
		if (ti < nTargetParticles)
		{
		  if (0)
		  {
		    // Fix particles at a location that gradually moves towards the target
			
			pPos[zIdx] = make_float4(
	        	(pTargetPos[ti].x*b + pOriginalPos[zIdx].x*a)/RESTORE_TIME,
			    (pTargetPos[ti].y*b + pOriginalPos[zIdx].y*a)/RESTORE_TIME,
				(pTargetPos[ti].z*b + pOriginalPos[zIdx].z*a)/RESTORE_TIME,
				0);

		  }
		  
		  if (0 && ti<10)
		  {
		      printf("Tracking particle %d: xyz=%f,%f,%f target=%f,%f,%f\n",
			    ti,pPos[zIdx].x,pPos[zIdx].y,pPos[zIdx].z,pTargetPos[ti].x,pTargetPos[ti].y,pTargetPos[ti].z);
		  }
		  
		  if (1)
		  {
			// Apply a constant-magnitude force directed towards where the particle should end up...
			float3 diff = make_float3(
	        	pTargetPos[ti].x - pPos[zIdx].x,
			    pTargetPos[ti].y - pPos[zIdx].y,
				pTargetPos[ti].z - pPos[zIdx].z);

			float dist = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);

			if (dist > 0.1f)
			{
				float f = RESTORE_FORCE_CONSTANT / dist;
			
				pAcc[zIdx].x += f*diff.x;
				pAcc[zIdx].y += f*diff.y;
				pAcc[zIdx].z += f*diff.z;
			}
		  }
		}

		
		if (pPos[zIdx].x >= MAXXE) {pPos[zIdx].x = MAXXE; pVel[zIdx].x = 0.0f;}
		if (pPos[zIdx].y >= MAXYE) {pPos[zIdx].y = MAXYE; pVel[zIdx].y = 0.0f;}
		if (pPos[zIdx].z >= MAXZE) {pPos[zIdx].z = MAXZE; pVel[zIdx].z = 0.0f;}
		if (pPos[zIdx].x < 0.0f) {pPos[zIdx].x = 0.0f; pVel[zIdx].x = 0.0f;}
		if (pPos[zIdx].y < 0.0f) {pPos[zIdx].y = 0.0f; pVel[zIdx].y = 0.0f;}
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

__global__ void ParticleForces(float4 *pPos, float4 *pVel, float4 *pAcc, unsigned *cellHash, int *cellStart, int nParticles, int nloops, unsigned *trackIndex)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	int neighbourCell,ps;
	int cellx,celly,cellz;
	
	float4 thisAcc;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		cellx = (int)(pPos[zIdx].x/CELL_DIMX); 
		celly = (int)(pPos[zIdx].y/CELL_DIMY);
		cellz = (int)(pPos[zIdx].z/CELL_DIMZ);

		thisAcc = make_float4(0.0f,0.0f,0.0f,0.0f);

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

							
						float dist = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
						if (dist < 1.0f)
						{
							
						    float f = 1.0f-dist;

							f = f*f * REPEL_FORCE_CONSTANT / dist;

							thisAcc.x += -f * diff.x;
							thisAcc.y += -f * diff.y;
							thisAcc.z += -f * diff.z;
							
						    if (dist<0.2 && 0) // debug output for too close particles
							{
							  printf("%d,%d separated by %f (x1,y1,z1=%f,%f,%f x2,y2,z2=%f,%f,%f force=%f,%f,%f)\n",trackIndex[zIdx],trackIndex[ps],dist,pPos[zIdx].x,pPos[zIdx].y,pPos[zIdx].z,pPos[ps].x,pPos[ps].y,pPos[ps].z,-f*diff.x,-f*diff.y,-f*diff.z);
							}

						}

					}
					ps++;
				}
			}
		}

		atomicAdd(&pAcc[zIdx].x,thisAcc.x);
		atomicAdd(&pAcc[zIdx].y,thisAcc.y);
		atomicAdd(&pAcc[zIdx].z,thisAcc.z);

		zIdx += stepSize;
	}
}


__global__ void ConnectForces(float4 *pPos, float4 *pVel, float4 *pAcc, unsigned *cellHash, int *cellStart, uint2 *neighbourList, float *neighbourDistance, int nParticlePairs, int nPPloops, unsigned *reverseTrackIndex, int iters)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nPPloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;
	
/*
	if (zIdx == 0 && iters%1==0)
	{
	  printf("Coordinates...\n");
	  for(int i = 0; i<2; i++)
	  {
	    printf("originalIndex=%d currentIndex=%d, x=%f, y=%f, z=%f\n",i,reverseTrackIndex[i],
		  pPos[reverseTrackIndex[i]].x,
		  pPos[reverseTrackIndex[i]].y,
		  pPos[reverseTrackIndex[i]].z);
	  }
	}
*/
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

//		if (iters%1==0) printf("Original particle pair: %d, %d\n",neighbourList[zIdx].x,neighbourList[zIdx].y);
//		if (iters%1==0) printf("Particle pair: %d, %d, distTarget:%f, dist:%f\n",p1,p2,distTarget,dist);
		
		float distFromTarget = dist - distTarget;

        float f = -distFromTarget*ATTRACT_FORCE_CONSTANT;

		float3 a = MultiplyVector(diff,f/dist);
		
//        if (iters%1==0) printf("xyz of %d is: %f,%f,%f\n",p1,pPos[p1].x,pPos[p1].y,pPos[p1].z);		
//        if (iters%1==0) printf("Force on %d is: %f,%f,%f\n",p1,a.x,a.y,a.z);		
		
		atomicAdd(&pAcc[p1].x,a.x);
		atomicAdd(&pAcc[p1].y,a.y);
		atomicAdd(&pAcc[p1].z,a.z);
	
		zIdx += stepSize;
	}
}

int nTargetParticles;
int nHoleParticles;
int nParticles;
int nParticlePairs;
float4 *d_pVel[2];
float4 *d_pPos[2];
float4 *d_pAcc;
float4 *d_pTargetPos;
float4 *d_pOriginalPos;
unsigned *d_cellHash;
int *d_cellStart;
unsigned *d_pIndex;
unsigned *d_trackIndex[2];
unsigned *d_reverseTrackIndex;
int *d_neighbourCount;       // How many neighbours does each particle have? Calculate this before allocating memory for below
uint2 *d_neighbourList;     // List of all pairs of particles p,q that are neighbours 
float *d_neighbourDistance;  // The distances between the neighbours in neighbourList


float4 *h_pVel;
float4 *h_pPos;
float4 *h_pTargetPos;
unsigned *h_trackIndex;
unsigned *h_reverseTrackIndex;
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
	cudaMemcpy(d_pOriginalPos,h_pPos,sizeof(float4)*nTargetParticles,cudaMemcpyHostToDevice);
	cudaMemcpy(d_pTargetPos,h_pTargetPos,sizeof(float4)*nTargetParticles,cudaMemcpyHostToDevice);
}

void CopyFromDevice(void)
{
	cudaMemcpy(h_pVel,d_pVel[activeArray],sizeof(float4)*nParticles,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_pPos,d_pPos[activeArray],sizeof(float4)*nParticles,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_trackIndex,d_trackIndex[activeArray],sizeof(unsigned)*nParticles,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_reverseTrackIndex,d_reverseTrackIndex,sizeof(unsigned)*nParticles,cudaMemcpyDeviceToHost);
}

void AllocateMemory(void)
{
	for(int i = 0; i<2; i++)
	{
		Check( cudaMalloc((void**)&d_pVel[i],sizeof(float4)*nParticles) );
		Check( cudaMalloc((void**)&d_pPos[i],sizeof(float4)*nParticles) );
		Check( cudaMalloc((void**)&d_trackIndex[i],sizeof(unsigned)*nParticles) );
	}
	Check( cudaMalloc((void**)&d_pTargetPos,sizeof(float4)*nTargetParticles) );
	Check( cudaMalloc((void**)&d_pOriginalPos,sizeof(float4)*nTargetParticles) );
	Check( cudaMalloc((void**)&d_pAcc,sizeof(float4)*nParticles) );
	Check( cudaMalloc((void**)&d_reverseTrackIndex,sizeof(unsigned)*nParticles) );
	Check( cudaMalloc((void**)&d_cellHash,sizeof(unsigned)*nParticles) );
	Check( cudaMalloc((void**)&d_cellStart,sizeof(int)*CELL_NUMX*CELL_NUMY*CELL_NUMZ) );
	Check( cudaMalloc((void**)&d_pIndex,sizeof(unsigned)*nParticles) );
	Check( cudaMalloc((void**)&d_neighbourCount,sizeof(int)*nParticles) );	
	
	h_pVel = (float4 *)malloc(sizeof(float4)*nParticles);
	h_pPos = (float4 *)malloc(sizeof(float4)*nParticles);
	h_pTargetPos = (float4 *)malloc(sizeof(float4)*nTargetParticles);
	h_trackIndex = (unsigned *)malloc(sizeof(unsigned)*nParticles);
	h_reverseTrackIndex = (unsigned *)malloc(sizeof(unsigned)*nParticles);
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

//	printf("numNeighbourParis:%d\n",numNeighbourPairs);
	
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
	cudaFree(d_pTargetPos);
	cudaFree(d_pOriginalPos);
	cudaFree(d_pAcc);
	cudaFree(d_reverseTrackIndex);
	cudaFree(d_cellHash);
	cudaFree(d_cellStart);
	cudaFree(d_pIndex);
	cudaFree(d_neighbourCount);
	cudaFree(d_neighbourList);
	cudaFree(d_neighbourDistance);
	free(h_pVel);
	free(h_pPos);
	free(h_pTargetPos);
	free(h_trackIndex);
	free(h_reverseTrackIndex);
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
		
		fclose(f);
	}

	return i;
}

// Two vectors that define the projection plane
// The third vector in this array is the normal to the projection plane, calculated from the other two
float planeVectors[3][3] = 
	{ { 1, 0, 0 },
	  { 0, 0, 1 } };

float projectN(float x, float y, float z, int n)
{
	float r = (x*planeVectors[n][0]+y*planeVectors[n][1]+z*planeVectors[n][2])/1000.0f;
	
	return r;
}

float inverseRotate[3][3];
float inverseTranslate[3];

float inverseProjectN(float x, float y, float z, int n)
{
	float r = (x*inverseRotate[n][0]+y*inverseRotate[n][1]+z*inverseRotate[n][2])/1000.0f;
	
	return r;
}

void InitialiseInverse(void)
{
//  printf("Initialising inverse\n");
  
  // computes the inverse of a matrix m
  double det = planeVectors[0][0] * (planeVectors[1][1] * planeVectors[2][2] - planeVectors[2][1] * planeVectors[1][2]) -
               planeVectors[0][1] * (planeVectors[1][0] * planeVectors[2][2] - planeVectors[1][2] * planeVectors[2][0]) +
               planeVectors[0][2] * (planeVectors[1][0] * planeVectors[2][1] - planeVectors[1][1] * planeVectors[2][0]);

  double invdet = 1000000.0f / det;

  inverseRotate[0][ 0] = (planeVectors[1][ 1] * planeVectors[2][ 2] - planeVectors[2][ 1] * planeVectors[1][ 2]) * invdet;
  inverseRotate[0][ 1] = (planeVectors[0][ 2] * planeVectors[2][ 1] - planeVectors[0][ 1] * planeVectors[2][ 2]) * invdet;
  inverseRotate[0][ 2] = (planeVectors[0][ 1] * planeVectors[1][ 2] - planeVectors[0][ 2] * planeVectors[1][ 1]) * invdet;
  inverseRotate[1][ 0] = (planeVectors[1][ 2] * planeVectors[2][ 0] - planeVectors[1][ 0] * planeVectors[2][ 2]) * invdet;
  inverseRotate[1][ 1] = (planeVectors[0][ 0] * planeVectors[2][ 2] - planeVectors[0][ 2] * planeVectors[2][ 0]) * invdet;
  inverseRotate[1][ 2] = (planeVectors[1][ 0] * planeVectors[0][ 2] - planeVectors[0][ 0] * planeVectors[1][ 2]) * invdet;
  inverseRotate[2][ 0] = (planeVectors[1][ 0] * planeVectors[2][ 1] - planeVectors[2][ 0] * planeVectors[1][ 1]) * invdet;
  inverseRotate[2][ 1] = (planeVectors[2][ 0] * planeVectors[0][ 1] - planeVectors[0][ 0] * planeVectors[2][ 1]) * invdet;
  inverseRotate[2][ 2] = (planeVectors[0][ 0] * planeVectors[1][ 1] - planeVectors[1][ 0] * planeVectors[0][ 1]) * invdet;	   

//  printf("Done initialising inverse\n");
  
}

void Initialise(char *target, char *flat, char *holes)
{
    FILE *f = fopen(target,"r");
	
	if(f)
	{
	    int i = 0;
		float x,y,z;
		float minx=1000000.0f,miny=1000000.0f,minz=1000000.0f;
	  
//	    printf("Loading...\n");
	    while(fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
	    {
			if (i<nParticles-nHoleParticles)
			{
				h_pTargetPos[i].x = projectN(x,y,z,0);
				h_pTargetPos[i].y = projectN(x,y,z,2);
				h_pTargetPos[i].z = projectN(x,y,z,1);
				
				if (h_pTargetPos[i].x < minx) minx = h_pTargetPos[i].x;
				if (h_pTargetPos[i].y < miny) miny = h_pTargetPos[i].y;
				if (h_pTargetPos[i].z < minz) minz = h_pTargetPos[i].z;
			}
			i++;
	    }
		
		inverseTranslate[0] = minx - XZ_OFFSET;
		inverseTranslate[1] = miny;
		inverseTranslate[2] = minz - XZ_OFFSET;
		
		for(i = 0; i<nParticles-nHoleParticles; i++)
		{
		   h_pTargetPos[i].x = h_pTargetPos[i].x - minx + XZ_OFFSET;
		   h_pTargetPos[i].y -= miny;
		   h_pTargetPos[i].z = h_pTargetPos[i].z - minz + XZ_OFFSET;
		}
		
//		printf("Loaded %d target particles\n",i);
		
		fclose(f);
	}

    f = fopen(flat,"r");

	if(f)
	{
	    int i = 0;
		float x,y,z;
		  
	    while(fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
	    {
			if (i<nParticles)
			{
				h_pVel[i].x = 0.0f;
				h_pVel[i].y = 0.0f;
				h_pVel[i].z = 0.0f;
				h_pPos[i].x = x;
				h_pPos[i].y = y;
				h_pPos[i].z = z;
			}
			i++;
	    }
		
//		printf("Loaded %d flat particles\n",i);
		
		fclose(f);
	}

    f = fopen(holes,"r");
	
	if(f)
	{
	    int i = 0;
		float x,y,z;
	  
//	    printf("Loading...\n");
	    while(fscanf(f,"%f,%f,%f",&x,&y,&z)==3)
	    {
			if (i<nHoleParticles)
			{
				h_pVel[i+nTargetParticles].x = 0.0f;
				h_pVel[i+nTargetParticles].y = 0.0f;
				h_pVel[i+nTargetParticles].z = 0.0f;
				h_pPos[i+nTargetParticles].x = x;
				h_pPos[i+nTargetParticles].y = y;
				h_pPos[i+nTargetParticles].z = z;
			}
			i++;
	    }
		
//		printf("Loaded %d hole particles\n",i);
		
		fclose(f);
	}
}

void Display(void)
{
//	printf(":T{%f}\n",h_simTime);
	float x,y,z;
	
    float maxy = 0.0f;
	
	for(int i = 0; i<nParticles; i++)
	{
	  if (h_pPos[i].y>maxy) maxy = h_pPos[i].y;
    }
	
//	printf("maxy=%f\n",maxy);
	
	
	for(int i = 0; i<nParticles; i++)
	{
		//printf(":P{%d,%.2f,%.2f,%.2f,%.4g,%.4g,%.4g,%.2f,%.2f,%.2f,%.2f}\n",h_trackIndex[i],h_pPos[i].x,h_pPos[i].y,h_pPos[i].z,h_pVel[i].x,h_pVel[i].y,h_pVel[i].z,0.5f,1.0f,1.0f,1.0f);

		// Output in the same order as the input particles
		
		int currentIndex = h_reverseTrackIndex[i];

		x = h_pPos[currentIndex].x;
		y = h_pPos[currentIndex].y;
		z = h_pPos[currentIndex].z;


		if (0 && i==0)
		  printf("0-pos: %.2f,%.2f,%.2f\n",x,y,z);
		
		x += inverseTranslate[0];
		y += inverseTranslate[1];
		z += inverseTranslate[2];

		if (0 && i==0)
		  printf("0-translated: %.2f,%.2f,%.2f\n",x,y,z);

		
		float xf = inverseProjectN(x,z,y,0);
		float yf = inverseProjectN(x,z,y,1);
		float zf = inverseProjectN(x,z,y,2);
		
		printf("%.2f,%.2f,%.2f\n",xf,yf,zf);
	}
	
}

int main(int argc, char *argv[])
{
	if (argc != 10)
	{
	  printf("Usage: surfaceUnFlatten <input.csv> <flat.csv> <holes.csv> x1 y1 z1 x2 y2 z2\n");
	  exit(1);
	}

	for(int i = 0; i<2; i++)
	  for(int j = 0; j<3; j++)
	  {
	    planeVectors[i][j] = atof(argv[j+i*3+argc-6]);
//		printf("%f\n",planeVectors[i][j]);
	  }
	  
//    printf("Projection plane and normal vectors normalised to length 1000:\n");
    for(int i = 0; i<3; i++)
    {
	  if (i==2)
	  {
	    /* Cross product of the two plane vectors gives the normal vector */
	    planeVectors[2][0] = (planeVectors[0][1]*planeVectors[1][2]-planeVectors[0][2]*planeVectors[1][1])/1000.0f;
	    planeVectors[2][1] = (planeVectors[0][2]*planeVectors[1][0]-planeVectors[0][0]*planeVectors[1][2])/1000.0f;
	    planeVectors[2][2] = (planeVectors[0][0]*planeVectors[1][1]-planeVectors[0][1]*planeVectors[1][0])/1000.0f;
      }
	
  	  float magnitude = sqrtf(planeVectors[i][0]*planeVectors[i][0]+planeVectors[i][1]*planeVectors[i][1]+planeVectors[i][2]*planeVectors[i][2]);
		
	  for(int j = 0; j<3; j++)
	  {
		planeVectors[i][j] /= (magnitude/1000.0f);
		
//		  printf("%f ",planeVectors[i][j]);
	  }
		
//	  printf("\n");
    }

	InitialiseInverse();
/*
	for(int i = 0; i<3; i++)
	  for(int j = 0; j<3; j++)
	  {
		printf("%f\n",inverseRotate[i][j]);
	  }
*/	
	nTargetParticles = GetNumParticles(argv[1]);
	nHoleParticles = GetNumParticles(argv[3]);
	nParticles = GetNumParticles(argv[2])+ nHoleParticles;
	
	if (nParticles != nTargetParticles+nHoleParticles)
	{
	  printf("Particle count mismatch\n");
	  exit(2);
	}
	
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

//	printf("About to allocate memory\n");
	AllocateMemory();
	Initialise(argv[1],argv[2],argv[3]);
	
/*
	printf("Inverse translation:\n");
	for(int i = 0; i<3; i++)
	{
      printf("%f\n",inverseTranslate[i]);
	}
*/	
	CopyToDevice();

	InitialiseDevice<<< nblks, nthds >>>(d_pPos[activeArray],d_pAcc,d_cellHash,d_pIndex,d_trackIndex[activeArray],d_reverseTrackIndex,nParticles,nloops);
	thrust::sort_by_key(	thrust::device_ptr<uint>(d_cellHash),
				thrust::device_ptr<uint>(d_cellHash+nParticles),
				thrust::device_ptr<uint>(d_pIndex));

								
	InitCellStart<<< nblks, nthds >>>(d_cellStart,nCellLoops);
	
	ArrayCopy<<< nblks, nthds >>>(d_pPos[activeArray],d_pPos[1-activeArray],d_pVel[activeArray],d_pVel[1-activeArray],d_cellHash,d_cellStart,d_pIndex,nParticles,nloops);

	UpdateTrackIndex<<< nblks, nthds >>>(d_pIndex,d_trackIndex[activeArray],d_trackIndex[1-activeArray],d_reverseTrackIndex,nParticles,nloops);
	
	activeArray = 1-activeArray;
 
    CountNeighbours<<< nblks, nthds >>>(d_pPos[activeArray],d_cellHash,d_cellStart,d_neighbourCount, d_pIndex,nParticles, nloops);

//	printf("About to allocate neighbour memory\n");
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

	int iters = RESTORE_TIME+RELAX_TIME;

	for(int i = 0; i<iters;i++)
	{
//	        printf("Iteration %d\n",i);

			ParticleForces<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pAcc,d_cellHash,d_cellStart,nParticles,nloops,d_trackIndex[activeArray]);

			ConnectForces<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pAcc,d_cellHash,d_cellStart,d_neighbourList,d_neighbourDistance,nParticlePairs,nPPloops,d_reverseTrackIndex,i);

			ParticleMove<<< nblks, nthds >>>(d_pPos[activeArray],d_pVel[activeArray],d_pAcc,d_pOriginalPos,d_pTargetPos,d_cellHash,d_trackIndex[activeArray]
			,d_pIndex,nParticles,nloops,nTargetParticles,i);
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
