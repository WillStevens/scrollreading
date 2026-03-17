#pragma once
/*
This implements a way of representing surfaces that makes it efficient to check for overlap and
do alignment of patch and surface.

A surface is represented as a collection of patches, without bothering about removing patch intersections (because rendering will deal with that).

Each patch is a list of:
qx,qy,vx,vy,vz

where qx,qy are the floating point quadmesh coords within the patch and
vx,vy,vz are floating point scroll voxel coords.

A surface is a collection of patches represented as a list of:
qx,qy,vx,vy,vz,pid

For convenvience, points are added and compressed one patch at a time, but when using the data this cannot be assumed - points and patches
could be in any order within a chunk. For example, the boundary tracking routines will sometimes rewrite an entire chunk as a single
compressed block containing all patches within that chunk.

Where pid is the id of the patch that the point belongs to and qx,qy are the patch-local quadmesh coordinates

The list is stored in a compressed chunked format, chunked on the vx,vy,vz coords,
compressed using blosc2.

*/

#include <tuple>
#include <vector>

#include "common_types.h"

typedef std::tuple<int,int,int> chunkIndex;
typedef std::tuple<int,int> indexChunkIndex;
typedef std::tuple<float,float,float,float,float,int> gridPoint;

typedef struct __attribute__((packed)) {
	float qx,qy;
	float vx,vy,vz;
	int patch;
} binaryGridPoint;

typedef struct
{
    int locationRootLength;
    char *location;
} BigPatch;

BigPatch *OpenBigPatch(const char *location);
void CloseBigPatch(BigPatch *z);
chunkIndex GetChunkIndex(float x, float y, float z);
std::vector<chunkIndex> GetAllPatchChunks(BigPatch *z);
void EraseChunk(BigPatch *z,chunkIndex ci);
// TODO - this could be done more efficiently by passing in the binary data as a parameter rather than using a vector
// This will be done later after the function is confirmed working correctly
void WritePatchPoints(BigPatch *z, chunkIndex ci, const std::vector<gridPoint> &gridPoints);
void ReadPatchPoints(BigPatch *z, chunkIndex ci, std::vector<gridPoint> &gridPoints);
/* Pick a random point by first picking a random chunk, then a random point within that chunk */
/* Not every point has an equal chance of being picked, because this doesn't take account of number of points in a chunk */
/* But for some purposes this doesn't matter */
/* This function was written for select a new seed from a boundary */
/* rn1,rn2 are randomly generated numbers to use for selecting the point */
gridPoint SelectRandomPoint(BigPatch *z, unsigned rn1, unsigned rn2);
void AddToBigPatch(BigPatch *z, Patch &p,int patchNum);

