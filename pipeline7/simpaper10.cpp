#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <dirent.h>

#include "parameters.h"

#include "bigpatch.h"
#include "patch_generator.h"
#include "align_patches.h"
#include "erasepoints.h"
#include "badpatchfinder.h"
#include "sliceanimrender.h"
#include "position_patches.h"

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

int main(int argc, char *argv[])
{
	char mode='n';
	if (argc!=1 && argc!=2)
	{
		fprintf(stderr,"Usage: %s [mode]\n",argv[0]);
		fprintf(stderr,"Where optional mode can be:\n");
		fprintf(stderr,"'skip' to skip patch generation and load patches from disk\n");
		exit(-1);
	}
	else
	{
		if (argc==2)
			mode = argv[1][0];
	}
	
	printf("Started\n");
    fflush(stdout);

    int acceptedCount=0,unalignedCount=0,acceptedWithSomeBadVariance=0;
	
	AlignmentMap *am = new AlignmentMap;
	std::map<int,Patch> *patches = new std::map<int,Patch>;
	
	if (mode=='n')
	{
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
		
		for(int i=0; i<200; i++)
		{
			MemInfo();
			printf("======== Patch %d ========\n",i);
			
			printf("Seed: %f,%f,%f,%f,%f,%f,%f,%f,%f,\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7],seed[8]);
			Patch patch,boundary;
			
			int steps = pg->GeneratePatch(seed,patch,boundary);
		    patch.radius = steps/2;
			
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
	}
	
	if (mode=='s') // load patches and alignments
	{
		DIR *dir;
		struct dirent *ent;
	
		// iterate through all patch files
		if ((dir = opendir ("patches")) != NULL)
		{
			while ((ent = readdir (dir)) != NULL)
			{
				std::string file(ent->d_name);
				
				int patchNum=0;
			
				if (file.length()>=4 && ends_with(file,".bin"))
				{			
					for(auto c : file)
					{
						if (isdigit(c))
							patchNum = patchNum*10+(c-'0');
					}
				
					Patch p;
		
					p.Read("patches",patchNum);
			
					(*patches)[patchNum]=p;
				}
			}
		}
		
		{
			std::ifstream is("patches/rel.csv");
			std::string line;
			
			while(std::getline(is,line))
			{
				std::stringstream ss(line);
				std::vector<float> row;
				std::string value;
				
				while(std::getline(ss,value,','))
				{
					row.push_back(std::stof(value));
				}

				if (am->count((int)row[0]) == 0)
				{
					(*am)[(int)row[0]] = std::vector<alignment>();
				}
				
				(*am)[(int)row[0]].push_back(alignment((int)row[1],
				                                            row[2],
				                                            row[3],
				                                            row[4],
				                                            row[5],
				                                            row[6],
				                                            row[7],
				                                            row[8],
				                                            row[9],
				                                            row[10],
				                                            row[11],
				                                            row[12],
				                                            row[13]));
			}
		}

	}
	
	std::set<int> badPatches;
	
    BadPatchFinder *bpf = new BadPatchFinder();
	bpf->FindBadPatches(*am,patches,badPatches);
	delete bpf;
	
	printf("Bad patches are:\n");
	for(auto i : badPatches)
	{
		printf("%d\n",i);
	}
	
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

	printf("Patch order is:\n");	 
	for (auto i : patchOrder)
	{
		printf("%d\n",i);
	}

	std::map<int,affineTx> patchPositions;
	std::vector<std::pair<int,alignment>> alignmentOrder;
	AugmentAlignmentMap(*am);
    PositionPatches(patches,*am,patchOrder,patchPositions,alignmentOrder);

	{
		ofstream os("alignmentorder.txt");
		
		for(auto &a : alignmentOrder)
		{
			os << a.first << " "
               << (*patches)[a.first].radius << " "			
			   << std::get<0>(a.second) << " "
			   << std::get<7>(a.second) << " "
			   << std::get<8>(a.second) << " "
			   << std::get<9>(a.second) << " "
			   << std::get<10>(a.second) << " "
			   << std::get<11>(a.second) << " "
			   << std::get<12>(a.second) << " "
			   << endl;
		}
	}

/*
	{
		ofstream os("patchPositions.txt");
		
		for(auto &pp : patchPositions)
		{
			float x,y,angle;
			AffineTxToXYA(pp.second,x,y,angle);
			os << pp.first << " " << x << " " << y << " " << angle << endl;
		}
	}
*/

	printf("Enter any character then press return after patchsprings has run\n");
	{ std::string tmp; std::cin >> tmp; }

	{
		ifstream is("patchPositions.txt");
		
		while(true)
		{
			int patchNum;
			float x,y,angle;
			if (is >> patchNum >> x >> y >> angle)
				(*patches)[patchNum].SetPosition(x,y,angle);
			else
				break;
		}
	}

	printf("Loaded patch positions\n");
	
	// Which patches could be in each x,y chunk?
	std::map<std::pair<int,int>,std::set<int>> patchIndex;
	int patchIndexScale = 16;
	
	{
		for (auto i : patchOrder)
		{
			Patch &p = (*patches)[i];
			for(auto &pt : p.points)
			{
				float x,y;
				p.TransformPoint(pt.x,pt.y,x,y);
				int xi = x/patchIndexScale;
				int yi = y/patchIndexScale;
				
				for(int xo = xi-1; xo <= xi+1; xo++)
				for(int yo = yi-1; yo <= yi+1; yo++)
				{
					if (patchIndex.count(std::pair<int,int>(xo,yo))==0)
						patchIndex[std::pair<int,int>(xo,yo)] = std::set<int>();
					
					patchIndex[std::pair<int,int>(xo,yo)].insert(i);
				}
					
			}
		}
	}
	
	printf("Finished indexing patches\n");
	
	for(auto &pi : patchIndex)
	{
		printf("%d,%d :",pi.first.first,pi.first.second);
		
		for(auto i : pi.second)
		{
			printf(" %d",i);
		}
		
		printf("\n");
	}
	
	// Now iterate over some coords...
	{
		Patch outputPatch;
		
		for(float x=-150; x<=150; x+=1)
		{
			printf("x=%f\n",x);
			for(float y=-150; y<=150; y+=1)
			{
				int xi = x/patchIndexScale;
				int yi = y/patchIndexScale;
				
				if (patchIndex.count(std::pair<int,int>(xi,yi))!=0)
				{
					Vec3 totalV;
					float totalWeight = 0.0;
								
					//printf("{\n");
					for(auto &i : patchIndex[std::pair<int,int>(xi,yi)])
					{
						Vec3 v;
						float weight=0.0;
						
						(*patches)[i].FindGlobalXY(x,y,v,weight);
					
						if (weight>0.0)
						{
							//printf("patch=%d v=%f,%f,%f weight=%f\n",i,v.x,v.y,v.z,weight);
							totalV += v*weight;
							totalWeight += weight;
						}
					}
					//printf("}\n");

					if (totalWeight != 0.0)
					{
						totalV /= totalWeight;
						outputPatch.points.push_back(patchPoint(x,y,totalV.x,totalV.y,totalV.z));
					}

					//printf("%f,%f has coords %f,%f,%f\n",x,y,totalV.x,totalV.y,totalV.z); 
					
				}
				
			}
		}
		
		outputPatch.Write(".",0);
	}

	exit(0);
	
	{
		// Enough buffers that we can do several layers before reloading buffers
		ZARR_1_b700 *surfaceZarr = ZARROpen_1_b700(SURFACE_ZARR);

		SliceAnimRender(surfaceZarr,std::string("sliceanim"),20,30,1,patches,patchOrder);
	
		ZARRClose_1_b700(surfaceZarr);
	}
	
	delete patches;
	delete am;
	
	exit(0);
}

