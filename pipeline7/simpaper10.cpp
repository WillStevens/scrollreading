#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "parameters.h"

#include "bigpatch.h"
#include "patch_generator.h"
#include "align_patches.h"
#include "erasepoints.h"
#include "badpatchfinder.h"
#include "sliceanimrender.h"

void MemInfo(void)
{
	std::ifstream is("/proc/meminfo");
	std::string s;
	
	while (is)
	{
		is >> s;
		if (s==std::string("MemFree:"))
		{
			std::cout << s;
			is >> s;
			std::cout << s << std::endl;
		}
	}
}

bool VarianceTest(float v0,float v1, float v2, float v3, float v4,float v5)
{
  return v0<=MAX_ROTATE_VARIANCE && v1<=MAX_ROTATE_VARIANCE && v2<=MAX_TRANSLATE_VARIANCE && v3<=MAX_ROTATE_VARIANCE && v4<=MAX_ROTATE_VARIANCE && v5<=MAX_TRANSLATE_VARIANCE;
}

// TODO - need to handle case when no point can be found
bool GetNewSeed(BigPatch *bp,BigPatch *bpb,float (&seed)[9])
{
	bool found = false;
	gridPoint newSeed;
	float dx01,dy01,dx02,dy02;
	int seedAxis0,seedAxis1;
	std::vector<gridPoint> neighbours;
	
	while(!found)
	{
		// Get a random point from the boundary
		newSeed = SelectRandomPoint(bpb,rand(),rand());
		printf("Selected %f,%f,%f\n",std::get<2>(newSeed),std::get<3>(newSeed),std::get<4>(newSeed));
        // Find any neighbours it has to help work out seed orientation
		neighbours.clear();
		neighbours.push_back(newSeed);
		FindBigPatchPointNeighbours(bp,newSeed,neighbours);
		
		// If it only has one neighbour, look for neighbours of this neighbour
		if (neighbours.size()==2)
		{
			gridPoint singleNeighbour = neighbours[1];
			neighbours.clear();
			neighbours.push_back(singleNeighbour);
		    FindBigPatchPointNeighbours(bp,singleNeighbour,neighbours);
		}
				
		// The neighbour[0] point should have neighbours in two different axis directions
		// If not, don't use this point
		if (neighbours.size()>=3)
		{
			seedAxis0 = 1; seedAxis1 = 2;
			dx01 = std::get<0>(neighbours[0])-std::get<0>(neighbours[1]);
			dy01 = std::get<1>(neighbours[0])-std::get<1>(neighbours[1]);
			
			dx02 = std::get<0>(neighbours[0])-std::get<0>(neighbours[2]);
			dy02 = std::get<1>(neighbours[0])-std::get<1>(neighbours[2]);
			
			// If the dot product of these is not close to zero, try some other possibilities
            if (DotProduct(dx01,dy01,dx02,dy02)<0.01)
			  found = true;
		    else
			  printf("DP=%f\n",DotProduct(dx01,dy01,dx02,dy02));
		  
		    if (!found && neighbours.size()>=4)
			{
				seedAxis1 = 3;
			    dx02 = std::get<0>(neighbours[0])-std::get<0>(neighbours[3]);
			    dy02 = std::get<1>(neighbours[0])-std::get<1>(neighbours[3]);
				
				if (DotProduct(dx01,dy01,dx02,dy02)<0.01)
				  found = true;
				else
				  printf("DP=%f\n",DotProduct(dx01,dy01,dx02,dy02));

			}
		}
	
		// Erase the selected point regardless of whether were going to use it, so that we don't select bad seeds again
        ErasePoints(bpb,std::get<2>(newSeed),std::get<3>(newSeed),std::get<4>(newSeed),0,CURRENT_BOUNDARY_ERASE_DISTANCE);

        // After all of that, if we find that the seed is near the edge of the volume, go back and pick another one  
        if (!(std::get<2>(newSeed)-VOL_OFFSET_X>8 && std::get<2>(newSeed)-VOL_OFFSET_X<VOL_SIZE_X-8 &&
  	        std::get<3>(newSeed)-VOL_OFFSET_Y>8 && std::get<3>(newSeed)-VOL_OFFSET_Y<VOL_SIZE_Y-8 &&
			std::get<4>(newSeed)-VOL_OFFSET_Z>8 && std::get<4>(newSeed)-VOL_OFFSET_Z<VOL_SIZE_Z-8))
          found = false;
	}
	seed[0] = std::get<2>(newSeed);
	seed[1] = std::get<3>(newSeed);
	seed[2] = std::get<4>(newSeed);
	Vec3 v(std::get<2>(neighbours[0])-std::get<2>(neighbours[seedAxis0]),
	       std::get<3>(neighbours[0])-std::get<3>(neighbours[seedAxis0]),
	       std::get<4>(neighbours[0])-std::get<4>(neighbours[seedAxis0]));
	Vec3 w(std::get<2>(neighbours[0])-std::get<2>(neighbours[seedAxis1]),
	       std::get<3>(neighbours[0])-std::get<3>(neighbours[seedAxis1]),
	       std::get<4>(neighbours[0])-std::get<4>(neighbours[seedAxis1]));
	v = v.normalized();
	w = w.normalized();
	seed[3] = v.x;
	seed[4] = v.y;
	seed[5] = v.z;
	seed[6] = w.x;
	seed[7] = w.y;
	seed[8] = w.z;
		
	return true;
}

int main(void)
{
	printf("Started\n");
    fflush(stdout);

    int acceptedCount=0,unalignedCount=0,acceptedWithSomeBadVariance=0;
	
	AlignmentMap *am = new AlignmentMap;
	std::map<int,Patch> *patches = new std::map<int,Patch>;
	PatchGenerator *pg = new PatchGenerator(string(SURFACE_ZARR),string(VECTORFIELD_ZARR));

	float seed[9] = {
      SEED_X,
	  SEED_Y,
	  SEED_Z,
	  SEED_AXIS1_X,
	  SEED_AXIS1_Y,
	  SEED_AXIS1_Z,
	  SEED_AXIS2_X,
	  SEED_AXIS2_Y,
	  SEED_AXIS2_Z};

	BigPatch *bp = OpenBigPatch("temp.bp");
	BigPatch *bpb = OpenBigPatch("tempb.bp");
	
	for(int i=0; i<20; i++)
	{
		MemInfo();
	    printf("======== Patch %d ========\n",i);
		
		printf("Seed: %f,%f,%f,%f,%f,%f,%f,%f,%f,\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7],seed[8]);
		Patch patch,boundary;
		
	    int steps = pg->GeneratePatch(seed,patch,boundary);
	
	    printf("%d growth steps\n",steps);
		
        if (i==0)
		{
			if (steps < MIN_PATCH_ITERS)
			{
				printf("Not enough growth steps (%d) on first seed\n",steps);
				exit(1);
			}
			
			AddToBigPatch(bp,patch,i);
			AddToBigPatch(bpb,boundary,i);
            (*patches)[i] = patch;									
		}			
		else if (steps >= MIN_PATCH_ITERS)
		{
			for(int alignAttempts = 0; alignAttempts<2; alignAttempts++)
			{
				Aligner *al = new Aligner();
			
				std::vector<alignment> alignments;
			
				al->AlignPatches(bp,patch,alignments);
			
				delete al;
			
				int numSuccessfulAlignments = 0, badVarianceCount = 9;
				for(auto const &a : alignments)
				{
					printf("%d (%f,%f,%f,%f,%f,%f) (%f,%f,%f,%f,%f,%f)\n",
						std::get<0>(a),
						std::get<1>(a),
						std::get<2>(a),
						std::get<3>(a),
						std::get<4>(a),
						std::get<5>(a),
						std::get<6>(a),
						std::get<7>(a),
						std::get<8>(a),
						std::get<9>(a),
						std::get<10>(a),
						std::get<11>(a),
						std::get<12>(a));
				  
					if (VarianceTest(std::get<1>(a),std::get<2>(a),std::get<3>(a),std::get<4>(a),std::get<5>(a),std::get<6>(a)))
					{
						numSuccessfulAlignments++;
						if (am->count(i)==0)
							(*am)[i] = std::vector<alignment>();
						(*am)[i].push_back(a);
                        (*patches)[i] = patch;						
					}
					else
						badVarianceCount++;
				}
			
				if (numSuccessfulAlignments)
				{
					acceptedCount++;
					if (badVarianceCount>0)
						acceptedWithSomeBadVariance++;
            
					// For the boundary we need to work out:
					// Given the new patch, which points from the current boundary should we delete?
					ErasePoints(bpb,patch,0,CURRENT_BOUNDARY_ERASE_DISTANCE);
					ErasePoints(bp,boundary,1,NEW_BOUNDARY_ERASE_DISTANCE);

					AddToBigPatch(bpb,boundary,i);
					AddToBigPatch(bp,patch,i);

					break;
				}
				else if (alignAttempts==0)
				{
					// Flip the patch and loop round for another try
					patch.Flip();
				}
				else
					unalignedCount++;
			}
		}
		else
		{
			printf("Not enough growth steps\n");
		}
		
		GetNewSeed(bp,bpb,seed);
	}
	
	CloseBigPatch(bpb);
	CloseBigPatch(bp);

	// Write patches and patch relationships to files
	for(auto &p : *patches)
	{
		p.second.Write("patches",p.first);
	}
	
	{
		std::ofstream os("patches/rel.csv");
		for(auto &a : *am)
		{
			for(auto &al : a.second)
			{
				os << a.first 
				   << "," << std::get<0>(al)
				   << "," << std::get<1>(al)
				   << "," << std::get<2>(al)
				   << "," << std::get<3>(al)
				   << "," << std::get<4>(al)
				   << "," << std::get<5>(al)
				   << "," << std::get<6>(al)
				   << "," << std::get<7>(al)
				   << "," << std::get<8>(al)
				   << "," << std::get<9>(al)
				   << "," << std::get<10>(al)
				   << "," << std::get<11>(al)
				   << "," << std::get<12>(al) << std::endl;
			}
		}
	}

	delete pg;
	
	std::set<int> badPatches;
	
    BadPatchFinder *bpf = new BadPatchFinder();
	bpf->FindBadPatches(*am,patches,badPatches);
	delete bpf;

/* Having identified bad patches, now we want too...
   - Starting from patch 0, generate a neighbouring patch sequence (lowest numbered patch first)
   - Slice render to help spot any more bad patches:
     parameters for slice render are: slices at a time (slicesper), Z-step (zstep), downscale (downscale)
	 */
	std::map<int,std::set<int> > neighbourList;
	int minPatchInNeighbourList = -1;
	
	for(auto &a : *am)
	{
		int patch1 = a.first;
	
		if (badPatches.count(patch1)==0)
		{
			// Iterate over alignments
			for(auto al : a.second)
			{	
				int patch2 = std::get<0>(al);

				if (badPatches.count(patch2)==0)
				{
					if (neighbourList.count(patch1)==0)
						neighbourList[patch1] = std::set<int>();
					if (neighbourList.count(patch2)==0)
						neighbourList[patch2] = std::set<int>();
					
					neighbourList[patch1].insert(patch2);
					neighbourList[patch2].insert(patch1);
					
					if (minPatchInNeighbourList==-1 || patch1<minPatchInNeighbourList)
						minPatchInNeighbourList = patch1;
					if (minPatchInNeighbourList==-1 || patch2<minPatchInNeighbourList)
						minPatchInNeighbourList = patch2;
				}
			}
		}
	}
	
	std::vector<int> patchOrder;
	std::set<int> visited;
	int currentVisit = minPatchInNeighbourList;
	std::set<int> toVisitSet;
	
	while(true)
	{
		visited.insert(currentVisit);

		patchOrder.push_back(currentVisit);
		
		std::set_difference(neighbourList[currentVisit].begin(),
						    neighbourList[currentVisit].end(),
							visited.begin(),
							visited.end(),
							std::inserter(toVisitSet,toVisitSet.begin()));
		
		if (toVisitSet.size()==0)
			break;
		
		auto nh = toVisitSet.extract(toVisitSet.begin());
		currentVisit = nh.value();
	}
	 
	for (auto i : patchOrder)
	{
		printf("%d\n",i);
	}

	{
		ZARR_1 *surfaceZarr = ZARROpen_1(SURFACE_ZARR);

		SliceAnimRender(surfaceZarr,std::string("sliceanim"),5,40,1,patches,patchOrder);
	
		ZARRClose_1(surfaceZarr);
	}
	
	delete patches;
	delete am;
	
	exit(0);
}

