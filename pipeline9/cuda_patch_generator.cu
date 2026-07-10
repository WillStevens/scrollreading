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

#include "parameters.h"

#define NUM_BLOCKS 4096
#define THREADS_PER_BLOCK 1024


#define PI 3.14159265358979323846264f

#define DEBUG_OUT

#define SHEET_SIZE (MAX_GROWTH_STEPS+2)

float4 *d_pVel;
float4 *d_pPos;
float4 *d_pAcc;
unsigned *d_active;

float4 *h_pVel;
float4 *h_pPos;
unsigned *h_active;

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

__device__ float DotProduct(float3 a, float3 b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

__global__ void InitialiseDevice(float4 *pPos, float4* pVel, float4 *pAcc, unsigned *active, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		pPos[zIdx] = make_float4(0.0f,0.0f,0.0f,0.0f);
		pVel[zIdx] = make_float4(0.0f,0.0f,0.0f,0.0f);
		pAcc[zIdx] = make_float4(0.0f,0.0f,0.0f,0.0f);
		active[zIdx] = 0;
		
		if (zIdx==(SHEET_SIZE/2)*SHEET_SIZE+SHEET_SIZE/2)
		{
			active[zIdx]=1;
			pPos[zIdx] = make_float4(0.0f,0.0f,0.0f,0.0f);
		}
		else if (zIdx==(SHEET_SIZE/2+1)*SHEET_SIZE+SHEET_SIZE/2)
		{
			active[zIdx]=1;
			pPos[zIdx] = make_float4(QUADMESH_SIZE,0.0f,0.0f,0.0f);
		}
		else if (zIdx==(SHEET_SIZE/2-1)*SHEET_SIZE+SHEET_SIZE/2)
		{
			active[zIdx]=1;
			pPos[zIdx] = make_float4(-QUADMESH_SIZE,0.0f,0.0f,0.0f);
		}
		else if (zIdx==(SHEET_SIZE/2)*SHEET_SIZE+SHEET_SIZE/2+1)
		{
			active[zIdx]=1;
			pPos[zIdx] = make_float4(0.0f,QUADMESH_SIZE,0.0f,0.0f);
		}
		else if (zIdx==(SHEET_SIZE/2)*SHEET_SIZE+SHEET_SIZE/2-1)
		{
			active[zIdx]=1;
			pPos[zIdx] = make_float4(0.0f,-QUADMESH_SIZE,0.0f,0.0f);
		}
		
		zIdx += stepSize;
	}
}

__global__ void ParticleMove(float4 *pPos, float4 *pVel, float4 *pAcc, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		if (active[zIdx])
		{
			pVel[zIdx].x += pAcc[zIdx].x;
			pVel[zIdx].y += pAcc[zIdx].y;
			pVel[zIdx].z += pAcc[zIdx].z;
			
			pVel[zIdx].x *= FRICTION_FORCE_CONSTANT;
			pVel[zIdx].y *= FRICTION_FORCE_CONSTANT;
			pVel[zIdx].z *= FRICTION_FORCE_CONSTANT;

			pPos[zIdx].x += pVel[zIdx].x;
			pPos[zIdx].y += pVel[zIdx].y;
			pPos[zIdx].z += pVel[zIdx].z;
			
			pAcc[zIdx] = make_float4(0.0f,0.0f,0.0f,0.0f);
		}
		
		zIdx += stepSize;
	}
}

__global__ void ParticleForces(float4 *pPos, float4 *pVel, float4 *pAcc, unsigned *active, int nParticles, int nloops)
{
	int thdsPerBlk = blockDim.x;	// Number of threads per block
	int blkIdx = blockIdx.x;		// Index of block
	int thdIdx = threadIdx.x;		// Index of thread within a block
	int zIdx = blkIdx*thdsPerBlk*nloops + thdIdx;
	int stepSize = blockDim.x*gridDim.x;
	
	float4 thisAcc;

	for(int i = 0; i<nloops && zIdx < nParticles; i++)
	{
		thisAcc = make_float4(0.0f,0.0f,0.0f,0.0f);

		if (active[zIdx])
		{
			x=i/SHEET_SIZE;
			y=i%SHEET_SIZE;
			
			if (x==0 || y==0 || x==SHEET_SIZE-1 || y==SHEET_SIZE-1)
			  break;
			  
			float4 posxy = pPos[zIdx];
			
			for(int xd=-1;xd<=1;xd++)
			for(int yd=-1;yd<=1;yd++)
			{
		      if (active[(x+xd)*SHEET_SIZE+y+yd])
	          {
		        float4 direction = posxy-pPos[(x+xd)*SHEET_SIZE+y+yd].pos;
                float actualDistance = LengthVector(direction)
				direction /= actualDistance;
 
				float force = expectedDistanceLookup[xd+1][yd+1]-actualDistance;

                thisAcc += direction*force;
			}

			pAcc[zIdx] = thisAcc*SPRING_FORCE_CONSTANT;
		}
	}
}

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
	cudaMemcpy(d_pVel,h_pVel,sizeof(float4)*SHEET_SIZE*SHEET_SIZE,cudaMemcpyHostToDevice);
	cudaMemcpy(d_pPos,h_pPos,sizeof(float4)*SHEET_SIZE*SHEET_SIZE,cudaMemcpyHostToDevice);
	cudaMemcpy(d_active,h_active,sizeof(float4)*SHEET_SIZE*SHEET_SIZE,cudaMemcpyHostToDevice);
}

void CopyFromDevice(void)
{
	cudaMemcpy(h_pVel,d_pVel,sizeof(float4)*SHEET_SIZE*SHEET_SIZE,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_pPos,d_pPos,sizeof(float4)*SHEET_SIZE*SHEET_SIZE,cudaMemcpyDeviceToHost);
	cudaMemcpy(h_active,d_active,sizeof(unsigned)*SHEET_SIZE*SHEET_SIZE,cudaMemcpyDeviceToHost);
}

void AllocateMemory(void)
{
	Check( cudaMalloc((void**)&d_pVel,sizeof(float4)*SHEET_SIZE*SHEET_SIZE) );
	Check( cudaMalloc((void**)&d_pPos,sizeof(float4)*SHEET_SIZE*SHEET_SIZE) );
	Check( cudaMalloc((void**)&d_pAcc,sizeof(float4)*SHEET_SIZE*SHEET_SIZE) );
	Check( cudaMalloc((void**)&d_active,sizeof(unsigned)*SHEET_SIZE*SHEET_SIZE) );
	
	h_pVel = (float4 *)malloc(sizeof(float4)*SHEET_SIZE*SHEET_SIZE);
	h_pPos = (float4 *)malloc(sizeof(float4)*SHEET_SIZE*SHEET_SIZE);
	h_active = (unsigned *)malloc(sizeof(unsigned)*SHEET_SIZE*SHEET_SIZE);
}

void FreeMemory(void)
{
	cudaFree(d_pVel);
	cudaFree(d_pPos);
	cudaFree(d_pAcc);
	cudaFree(d_active);
	free(h_pVel);
	free(h_pPos);
	free(h_active);
}

int main(int argc, char *argv[])
{
	if (argc != 1)
	{
	  printf("Usage: cuda_patch_generator\n");
	  exit(1);
	}

	int nthds = THREADS_PER_BLOCK;
	int nblks = NUM_BLOCKS;
	int nloops = 1+(SHEET_SIZE*SHEET_SIZE-1)/(nthds*nblks);

	float time = 0.0f;

	cudaSetDevice(0);

	cudaEvent_t start,stop;
	cudaEventCreate(&start);
	cudaEventCreate(&stop);

	AllocateMemory();

	//CopyToDevice();

	InitialiseDevice<<< nblks, nthds >>>(d_pPos,d_pVel,d_pAcc,d_active,SHEET_SIZE*SHEET_SIZE,nloops);

	cudaEventRecord(start,0);

	int iters = 10;

	for(int i = 0; i<iters;i++)
	{
	        printf("Iteration %d\n",i);

			ParticleForces<<< nblks, nthds >>>(d_pPos,d_pVel,d_pAcc,d_active,SHEET_SIZE*SHEET_SIZE,nloops]);

			ParticleMove<<< nblks, nthds >>>(d_pPos,d_pVel,d_pAcc,d_active,SHEET_SIZE*SHEET_SIZE,nloops);
	}
		

	CopyFromDevice();
	
	cudaEventRecord(stop,0);
	cudaEventSynchronize(stop);
	cudaEventElapsedTime(&time,start,stop);

	printf("Time taken for simulation part: %f for %d iters\n",time,iters);

	FreeMemory();
	return 0;
}
