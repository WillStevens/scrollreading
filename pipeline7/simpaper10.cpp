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
#include "zarr_show2_u8.h"

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
		if (!SelectRandomPoint(bpb,rand(),rand(),newSeed))
			return false;
		
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

	// Show what the neighbours are - useful fo debugging floating point exception error
	printf("Neighbours\n");
	for(auto &n : neighbours)
	{
		printf("%f,%f,%f,%f,%f,%d\n",std::get<0>(n),std::get<1>(n),std::get<2>(n),std::get<3>(n),std::get<4>(n),std::get<5>(n));
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

// Since most time is now taken by up patch generation, this could be made multithreaded by generating several seeds at a time,
// and if they are spaced far enough apart then generate several patches at once.
//
// This can be done by having more than once instance of PatchGenerator
void GeneratePatches(std::map<int,Patch> *patches,AlignmentMap *am)
{
	int acceptedCount=0,unalignedCount=0,acceptedWithSomeBadVariance=0;

	PatchGenerator *pg = new PatchGenerator(string(SURFACE_ZARR));
	
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

	BigPatch *bp = OpenBigPatch(OUTPUT_DIR "/surface.bp");
	BigPatch *bpb = OpenBigPatch(OUTPUT_DIR "/boundary.bp");

	int startingPatch = -1;
	
	for(auto &p : *patches)
	{
		if (p.first > startingPatch)
			startingPatch = p.first;
	}
	
	startingPatch++;

	// If we are restarting, we need to choose a new seed (overwrite what we set above)
	if (startingPatch>0)
	{
		if (!GetNewSeed(bp,bpb,seed))
			return;			
	}
	
	for(int i=startingPatch; i<startingPatch+30; i++)
	{
		MemInfo();
		printf("======== Patch %d ========\n",i);
		
		printf("Seed: %f,%f,%f,%f,%f,%f,%f,%f,%f,\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7],seed[8]);
		Patch boundary;
		
		(*patches)[i] = Patch();
		
		int steps = pg->GeneratePatch(seed,(*patches)[i],boundary,i);
		(*patches)[i].radius = steps/2;
		
		printf("%d growth steps\n",steps);

/*
	If this is the first iteration, generate only one patch
	otherwise generate several at once:
	int p = omp_get_max_threads();
	int steps[4];
	
	#pragma omp parallel num_threads(p)
	{
		int thrd = omp_get_thread_num();
		
		steps[thrd] = pg[thrd]->GeneratePatch(seed[thrd],newPatch[thrd],boundary[thrd],i+thrd);
		newPatch[thrd].radius = steps/2;
	}
	
	for(int thrd = 0; thrd<p; thrd++)
	{
		printf("Patch %d had %d growth steps\n",i+thrd,steps[thrd]);
	}
	
*/
		if (i==0)
		{
			if (steps < MIN_PATCH_ITERS)
			{
				printf("Not enough growth steps (%d) on first seed\n",steps);
				exit(1);
			}
			
			printf("Adding patch to bigpatch\n");
			AddToBigPatch(bp,(*patches)[i],i);
			printf("Adding boundary to bigpatch\n");
			AddToBigPatch(bpb,boundary,i);	

			printf("Added to bigpatch on first iteration");
			
			// Code for checking that iterating counts the same number of points as counting all points in pointGrid */
			/*
			{
				int count = 0,count1 = 0;
				for(PatchIterator pi = (*patches)[i].Begin(); (*patches)[i].Next(pi);)
				{
					count++;
				}					
				
				for(int x=0; x<=(*patches)[i].maxux-(*patches)[i].minux; x++)
				for(int y=0; y<=(*patches)[i].maxuy-(*patches)[i].minuy; y++)
				{
					if ((*patches)[i].pointGrid[x][y]) count1++;
				}
				
				printf("%d %d\n",count,count1);
				exit(0);
			}
			*/
			// Code for checking that normal calculation looks plausible
			/*{
				Vec3 n;
				(*patches)[i].GetNormal(0,0,n);
				
				printf("%f,%f,%f\n",n.x,n.y,n.z);
			}*/
		}			
		else if (steps >= MIN_PATCH_ITERS)
		{
			for(int alignAttempts = 0; alignAttempts<2; alignAttempts++)
			{
				Aligner *al = new Aligner();
			
				std::vector<alignment> alignments;
			
				al->AlignPatches(bp,(*patches)[i],alignments);
			
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
					ErasePoints(bpb,(*patches)[i],0,CURRENT_BOUNDARY_ERASE_DISTANCE);
					ErasePoints(bp,boundary,1,NEW_BOUNDARY_ERASE_DISTANCE);

					AddToBigPatch(bpb,boundary,i);
					AddToBigPatch(bp,(*patches)[i],i);

					break;
				}
				else if (alignAttempts==0)
				{
					// Flip the patch and loop round for another try
					(*patches)[i].Flip();
				}
				else
				{
					unalignedCount++;
					patches->erase(i);
				}
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
		p.second.Write(OUTPUT_DIR "/patches",p.first);
	}
	
	{
		std::ofstream os(OUTPUT_DIR "/rel.csv");
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

	printf("Deleting pg\n");
	delete pg;
	printf("Deleting patches\n");
}

void LoadPatchesAndRelationships(std::map<int,Patch> *patches, 	AlignmentMap *am, int limit = -1)
{
	DIR *dir;
	struct dirent *ent;

	// iterate through all patch files
	if ((dir = opendir (OUTPUT_DIR "/patches")) != NULL)
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
			
				if (patchNum<=limit || limit==-1)
				{
					(*patches)[patchNum]=Patch();
	
					printf("Loading %d\n",patchNum);
					(*patches)[patchNum].Read(OUTPUT_DIR "/patches",patchNum);
		
				}
			}
		}
		
		closedir(dir);
	}
	
	{
		std::ifstream is(OUTPUT_DIR "/rel.csv");
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

			if (limit==-1 || ( (int)row[0] <= limit && (int)row[1] <= limit) )
			{
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
		
		for(auto &a : *am)
		{
			printf("%d\n",a.first);
			for(auto &al : a.second)
			{
				printf(".%d\n",std::get<0>(al));
			}
		}
	}
	
}

int main(int argc, char *argv[])
{
	char mode='g';
	if (argc!=1 && argc!=2 && argc!=3)
	{
		fprintf(stderr,"Usage: %s [mode]\n",argv[0]);
		fprintf(stderr,"Where optional mode can be:\n");
		fprintf(stderr,"g - default : generation, write patches and relationships\n");
		fprintf(stderr,"r - restart from existing patches and relationships\n");
		fprintf(stderr,"b - identify bad patches, write list to file\n");
		fprintf(stderr,"v - generate a visit order, and augment alignment list with inverses\n");
		fprintf(stderr,"f - flatten, using patch positions as input\n");
		exit(-1);
	}
	else
	{
		if (argc>=2)
			mode = argv[1][0];
	}
	
	printf("Started\n");
    fflush(stdout);

	srand(RANDOM_SEED);
	
	if (mode=='g')
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		GeneratePatches(patches,am);
		printf("Generated patches\n");
		
		delete patches;
		delete am;
	}

	if (mode=='r')
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);

		GeneratePatches(patches,am);
		printf("Generated patches\n");
		
		delete patches;
		delete am;
	}
	
	if (mode=='b')
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);

		std::set<int> badPatches;
		std::vector<std::tuple<int,int,float>> badPatchScores;
	
		BadPatchFinder *bpf = new BadPatchFinder();
		printf("Finding bad patches...\n");
		bpf->FindBadPatches(*am,patches,badPatches,badPatchScores);
		delete bpf;

		{
			std::ofstream os(OUTPUT_DIR "/badpatches.csv");
			for(auto i : badPatches)
			{
				os << i << std::endl;;
			}
		}
		{
			std::ofstream os(OUTPUT_DIR "/badpatchscores.csv");
			for(auto i : badPatchScores)
			{
				os << std::get<0>(i) << "," << std::get<1>(i) << "," << std::get<2>(i) << std::endl;;
			}
		}
		
		printf("Finished, cleaning up...\n");
		delete patches;
		delete am;
	}

	if (mode=='c')
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,4000);
		
		std::set<int> badPatches;
		std::vector<std::tuple<int,int,float>> badPatchScores;
	
		BadPatchFinder *bpf = new BadPatchFinder();
		printf("Finding bad patches...\n");
		
		bpf->FindBadPatchesGeneral(*am,patches,2,badPatches,badPatchScores);		
		
		std::set<int> round1BadPatches = badPatches;
		
		bpf->FindBadPatchesGeneral(*am,patches,3,badPatches,badPatchScores);		
		std::set<int> round2OnlyBadPatches;
		
		std::set_difference(badPatches.begin(), badPatches.end(), round1BadPatches.begin(), round1BadPatches.end(),
                        std::inserter(round2OnlyBadPatches, round2OnlyBadPatches.begin()));
		
		printf("Round 1 bad patches\n");
		for(auto i : round1BadPatches)
		{
			printf("%d\n",i);
		}

		printf("Round 2 bad patches\n");
		for(auto i : round2OnlyBadPatches)
		{
			printf("%d\n",i);
		}

		{
			std::ofstream os(OUTPUT_DIR "/badpatches.csv");
			for(auto i : round1BadPatches)
			{
				os << i << std::endl;;
			}
			for(auto i : round2OnlyBadPatches)
			{
				os << i << std::endl;;
			}
		}
		{
			std::ofstream os(OUTPUT_DIR "/badpatchscores.csv");
			for(auto i : badPatchScores)
			{
				os << std::get<0>(i) << "," << std::get<1>(i) << "," << std::get<2>(i) << std::endl;;
			}
		}

		
		delete bpf;
		printf("Finished, cleaning up...\n");
		delete patches;
		delete am;
	}
	
	if (mode=='v')
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,4000);
		
		std::set<int> badPatches;

		{
			int i;
			
			std::ifstream is(OUTPUT_DIR "/badpatches.csv");
			while(is>>i)
			{
				badPatches.insert(i);
			}
		}

		/* Having identified bad patches, now we want too...
	   - Starting from patch 0, generate a neighbouring patch sequence (lowest numbered patch first)
	   - Slice render to help spot any more bad patches:
		 parameters for slice render are: slices at a time (slicesper), Z-step (zstep), downscale (downscale)
		 */
		std::map<int,std::set<int> > neighbourList;
		int minPatchInNeighbourList = -1;
		
		printf("Constructing neighbour list\n");
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

		printf("Calculating patch order...\n");
		
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

		{
			std::ofstream os(OUTPUT_DIR "/patchorder.csv");
			for (auto i : patchOrder)
			{
				os << i << std::endl;
			}
		}
		
		printf("Producing alignment order...\n");

		std::map<int,affineTx> patchPositions;
		std::vector<std::pair<int,alignment>> alignmentOrder;
		AugmentAlignmentMap(*am);
		PositionPatches(patches,*am,patchOrder,patchPositions,alignmentOrder);

		{
			ofstream os(OUTPUT_DIR "/alignmentorder.txt");
			
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
		
		delete patches;
		delete am;
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

	if (mode=='f')
	{
		int maxDistanceThresh = -1;
		
		if (argc==3)
			maxDistanceThresh = atoi(argv[2]);
		
		std::map<int,int> distanceDistribution;
		
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,4000);

		std::vector<int> patchOrder;
		
		{
			std::ifstream is(OUTPUT_DIR "/patchorder.csv");
			int i;
			while(is>>i)
			{
				patchOrder.push_back(i);
			}
		}

		// This must come from patchsprings.py
		// TODO - patchsprings will be rewritten in C++ soon
		ifstream is(OUTPUT_DIR "/patchPositions.txt");
		
		while(true)
		{
			int patchNum;
			float x,y,angle;
			if (is >> patchNum >> x >> y >> angle)
				(*patches)[patchNum].SetPosition(x,y,angle);
			else
				break;
		}

		printf("Loaded patch positions\n");
		
		// Which patches could be in each x,y chunk?
		std::map<std::pair<int,int>,std::set<int>> patchIndex;
		int patchIndexScale = 16;
		
		{
			for (auto i : patchOrder)
			{
				Patch &p = (*patches)[i];
				for(PatchIterator pi = p.Begin(); p.Next(pi);)
				{
					float x,y;
					p.TransformPoint(pi.p->x,pi.p->y,x,y);
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
		
		/*for(auto &pi : patchIndex)
		{
			printf("%d,%d :",pi.first.first,pi.first.second);
			
			for(auto i : pi.second)
			{
				printf(" %d",i);
			}
			
			printf("\n");
		}*/
		
		// Now iterate over some coords...
		{
			Patch outputPatch;
			std::vector<patchPoint> points;
			
			for(float x=-1600; x<=900; x+=1)
			{
				for(float y=-2600; y<=1400; y+=1)
				{
					std::vector<std::pair<int,Vec3>> contributions;
					
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
							
							if ((*patches)[i].FindGlobalXY(x,y,v,weight))						
								if (weight>0.0)
								{
									//printf("patch=%d v=%f,%f,%f weight=%f\n",i,v.x,v.y,v.z,weight);
									totalV += v*weight;
									totalWeight += weight;
								
									contributions.push_back(std::pair<int,Vec3>(i,v));
								}
						}
						//printf("}\n");

						if (totalWeight != 0.0)
						{
							totalV /= totalWeight;
							points.push_back(patchPoint(x,y,totalV.x,totalV.y,totalV.z));
						}

						//printf("%f,%f has coords %f,%f,%f\n",x,y,totalV.x,totalV.y,totalV.z); 
					
						// Look for the largest distance between pairs of contributions
						float maxDistance = 0;
						for(int i=0; i<contributions.size(); i++)
						{
							for(int j=i+1; j<contributions.size(); j++)
							{
								float distance = (contributions[i].second-contributions[j].second).length();
								if (distance > maxDistance)
								{
									maxDistance = distance;
								}
								
								int distance_i = (int)distance;
								
								if (distanceDistribution.count(distance_i)==0)
								{
									distanceDistribution[distance_i] = 0;
								}
								
								distanceDistribution[distance_i]++;
							}
						}
						
						if (maxDistanceThresh!=-1 && maxDistance>(float)maxDistanceThresh)
						{
							printf("Distance violation (%f) when flattening at %f,%f\n",maxDistance,x,y);
							for(auto &c : contributions)
							{
								printf("%d : %f,%f,%f\n",c.first,c.second.x,c.second.y,c.second.z);
							}
						}
					}
					
				}
			}
			
			outputPatch.BuildFromPoints(points);
			outputPatch.Write(OUTPUT_DIR,0);
			
			for(const auto &dd : distanceDistribution)
			{
				printf("%d,%d\n",dd.first,dd.second);
			}
		}
		
		delete patches;
		delete am;
	}
	
	if (mode=='a')
	{
		int closeUpIter = -1;
		
		if (argc==3)
			closeUpIter = atoi(argv[2]);
		
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);

		std::vector<int> patchOrder;
		
		{
			std::ifstream is(OUTPUT_DIR "/patchorder.csv");
			int i;
			while(is>>i)
			{
				patchOrder.push_back(i);
			}
		}

		// Enough buffers that we can do several layers before reloading buffers,
		ZARR_1_b700 *surfaceZarr = ZARROpen_1_b700(SURFACE_ZARR);

		printf("Rendering...\n");

		SliceAnimRender(surfaceZarr,std::string(OUTPUT_DIR "/sliceanim"),100,50,1,closeUpIter,patches,patchOrder);
	
		ZARRClose_1_b700(surfaceZarr);
	}

	if (mode=='p')
	{
		std::vector<int> patchesToShow;
		
		for(int i = 2; i<argc; i++)
			patchesToShow.push_back(i);
		
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);
		
		// Enough buffers that we can do several layers before reloading buffers,
		ZARR_1_b700 *surfaceZarr = ZARROpen_1_b700(SURFACE_ZARR);

		printf("Rendering...\n");

		SliceAnimRender(surfaceZarr,std::string(OUTPUT_DIR "/sliceprobe"),patchesToShow.size(),20,1,-1,patches,patchesToShow);
	
		ZARRClose_1_b700(surfaceZarr);
	}

	if (mode=='s')
	{		
		int zcoord = SEED_Z;
		
		if (argc>2)
			zcoord = atoi(argv[2]);
		
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);
	
		std::vector<Patch *> patchesToShow;
		
		for(auto &p : *patches)
			patchesToShow.push_back(&p.second);

		std::set<Patch *> shown;
		
		// Enough buffers that we can do several layers before reloading buffers,
		ZARR_1_b700 *surfaceZarr = ZARROpen_1_b700(SURFACE_ZARR);

		printf("Rendering...\n");
		
		ZarrShow2U8(surfaceZarr, 0,0,zcoord,VOL_SIZE_X,VOL_SIZE_Y,std::string(OUTPUT_DIR "/slice.tif"),patchesToShow,shown,0,0,0);
	
		ZARRClose_1_b700(surfaceZarr);
	}
	
	printf("Done\n");
	exit(0);
}

