#include <cstdio>
#include <cstdlib>
#include <tuple>

#include "parameters.h"

#include "bigpatch.h"
#include "patch_generator.h"
#include "align_patches.h"
#include "erasepoints.h"

typedef std::map<int,std::vector<alignment> > AlignmentMap;

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
		  
		    if (!found && neighbours.size()>=4)
			{
				seedAxis1 = 3;
			    dx02 = std::get<0>(neighbours[0])-std::get<0>(neighbours[3]);
			    dy02 = std::get<1>(neighbours[0])-std::get<1>(neighbours[3]);
				
				if (DotProduct(dx01,dy01,dx02,dy02)<0.01)
				  found = true;

			}
		}
		
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
	
	PatchGenerator *pg = new PatchGenerator(string(SURFACE_ZARR),string(VECTORFIELD_ZARR));
	AlignmentMap *am = new AlignmentMap;
	
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
	
	for(int i=0; i<10; i++)
	{
		printf("Seed: %f,%f,%f,%f,%f,%f,%f,%f,%f,\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7],seed[8]);
		Patch patch,boundary;
		
		/*Remove seed from boundary*/
	    pg->GeneratePatch(seed,patch,boundary);
		
	
        if (i==0)
		{
			AddToBigPatch(bp,patch,i);
			AddToBigPatch(bpb,boundary,i);
		}			
		else
		{
			Aligner *al = new Aligner();
			
			std::vector<alignment> alignments;
			
			al->AlignPatches(bp,patch,alignments);
			
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
			}
			else
				unalignedCount++;
		}
		
		GetNewSeed(bp,bpb,seed);
	}
	
	CloseBigPatch(bpb);
	CloseBigPatch(bp);
	
	delete am;
	delete pg;
}

