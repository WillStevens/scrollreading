#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <set>
#include <list>
#include <queue>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <dirent.h>
#include <random>

#include <omp.h>

#include "parameters.h"

#include "bigpatch.h"
#include "patch_generator.h"
#include "align_patches.h"
#include "erasepoints.h"
#include "badpatchfinder.h"
#include "sliceanimrender.h"
#include "patchcolourkey.h"
#include "position_patches.h"
#include "zarr_show2_u8.h"
#include "PatchSpringSimulation.hpp"
#include "anneal.h"
#include "visitorder.h"
#include "scoreplacement.h"
#include "omissiontest.h"

#define PATCH_LIMIT 10000
#define NUM_THREADS 4
#define MIN_SEED_DISTANCE 600

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

void EraseSeedPoint(BigPatch *bpb, float x, float y, float z)
{
	ErasePoints(bpb,x,y,z,0,CURRENT_BOUNDARY_ERASE_DISTANCE);
}

// TODO - need to handle case when no point can be found
bool GetNewSeed(BigPatch *bp,BigPatch *bpb,std::vector<float> &seed, bool erase = true)
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
		

		printf("Seed neighbours:%d\n",(int)neighbours.size());
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
			
        // After all of that, if we find that the seed is near the edge of the volume, go back and pick another one  
        if (!(std::get<2>(newSeed)-VOL_OFFSET_X>8 && std::get<2>(newSeed)-VOL_OFFSET_X<VOL_SIZE_X-8 &&
  	        std::get<3>(newSeed)-VOL_OFFSET_Y>8 && std::get<3>(newSeed)-VOL_OFFSET_Y<VOL_SIZE_Y-8 &&
			std::get<4>(newSeed)-VOL_OFFSET_Z>8 && std::get<4>(newSeed)-VOL_OFFSET_Z<VOL_SIZE_Z-8))
		{
          found = false;
		  printf("Seed was outside of volume\n");
		}
		
		if (erase || !found)
		{
			printf("Erase:%d found:%d\n",(int)erase,(int)found);
			// Erase the selected point regardless of whether we're going to use it, so that we don't select bad seeds again
			EraseSeedPoint(bpb,std::get<2>(newSeed),std::get<3>(newSeed),std::get<4>(newSeed));
		}

	}

	// Show what the neighbours are - useful fo debugging floating point exception error
	printf("Neighbours\n");
	for(auto &n : neighbours)
	{
		printf("%f,%f,%f,%f,%f,%d\n",std::get<0>(n),std::get<1>(n),std::get<2>(n),std::get<3>(n),std::get<4>(n),std::get<5>(n));
	}
	
	seed.push_back(std::get<2>(newSeed));
	seed.push_back(std::get<3>(newSeed));
	seed.push_back(std::get<4>(newSeed));
	Vec3 v(std::get<2>(neighbours[0])-std::get<2>(neighbours[seedAxis0]),
	       std::get<3>(neighbours[0])-std::get<3>(neighbours[seedAxis0]),
	       std::get<4>(neighbours[0])-std::get<4>(neighbours[seedAxis0]));
	Vec3 w(std::get<2>(neighbours[0])-std::get<2>(neighbours[seedAxis1]),
	       std::get<3>(neighbours[0])-std::get<3>(neighbours[seedAxis1]),
	       std::get<4>(neighbours[0])-std::get<4>(neighbours[seedAxis1]));
	v = v.normalized();
	w = w.normalized();
	seed.push_back(v.x);
	seed.push_back(v.y);
	seed.push_back(v.z);
	seed.push_back(w.x);
	seed.push_back(w.y);
	seed.push_back(w.z);
		
	return true;
}

// no two seeds should be closer than specified distance
bool CheckSeedDistances(std::vector<std::vector<float>> &seeds)
{
	for(size_t i = 0; i<seeds.size(); i++)
	{
		for(size_t j = i+1; j<seeds.size(); j++)
		{
			if (Distance(seeds[i][0],seeds[i][1],seeds[i][2],seeds[j][0],seeds[j][1],seeds[j][2]) < MIN_SEED_DISTANCE)
				return false;
		}
	}
	
	return true;
}

// Since most time is now taken by up patch generation, this could be made multithreaded by generating several seeds at a time,
// and if they are spaced far enough apart then generate several patches at once.
//
// This is done by having more than once instance of PatchGenerator
bool GeneratePatches(std::map<int,Patch> *patches,AlignmentMap *am, int numPatches)
{
	int acceptedCount=0,unalignedCount=0,acceptedWithSomeBadVariance=0;

	PatchGenerator *pg[NUM_THREADS];
	
	for(int i = 0; i<NUM_THREADS; i++)
	{
		pg[i] = new PatchGenerator(string(SURFACE_ZARR));
	}
	
	std::vector<std::vector<float> > seeds;
			
	float seedInit[] = {
	  SEED_X,
	  SEED_Y,
	  SEED_Z,
	  SEED_AXIS1_X,
	  SEED_AXIS1_Y,
	  SEED_AXIS1_Z,
	  SEED_AXIS2_X,
	  SEED_AXIS2_Y,
	  SEED_AXIS2_Z};

	seeds.push_back(std::vector<float>(std::begin(seedInit),std::end(seedInit)));
	  
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
		seeds.clear();
		seeds.push_back(std::vector<float>());
		if (!GetNewSeed(bp,bpb,seeds.back()))
		{
			seeds.pop_back();
			return false;
		}
	}
	
	for(int i=startingPatch; i<=startingPatch+numPatches;)
	{
		MemInfo();
		printf("======== Patch %d ========\n",i);

		if (i != startingPatch)
		{
			seeds.clear();
			
			for(int j=0; j<2*NUM_THREADS && seeds.size()<NUM_THREADS; j++)
			{
				seeds.push_back(std::vector<float>());
				if (!GetNewSeed(bp,bpb,seeds.back(),j==0))
				{
					seeds.pop_back();
					break;
				}
				
				if (j>0)
				{
					// make sure that that the new seed is sufficiently far from previous seeds.
					if (!CheckSeedDistances(seeds))
					{
						// if not then discard it
						printf("Seed did not meet distance requirement\n");
						seeds.pop_back();
					}
					else
					{
						printf("Seed met distance requirement\n");
						// if yes then keep it, and because it will be used we need to erase it from the boundary.
						EraseSeedPoint(bpb,seeds.back()[0],seeds.back()[1],seeds.back()[2]);
					}
				}
			}
		}
	
		if (seeds.size()>0)
		{
			
			printf("Generated %d seeds\n",(int)seeds.size());
			
			for(auto &seed : seeds)
			{
				printf("Seed: %f,%f,%f,%f,%f,%f,%f,%f,%f,\n",seed[0],seed[1],seed[2],seed[3],seed[4],seed[5],seed[6],seed[7],seed[8]);
			}
			
			Patch boundary[NUM_THREADS];
			int steps[NUM_THREADS];

			int N = seeds.size();

			for(int patchGenNum = 0; patchGenNum<N; patchGenNum++)
			{
				(*patches)[i+patchGenNum] = Patch();
			}
			
			#pragma omp parallel for schedule(dynamic)
			for(int patchGenNum = 0; patchGenNum<N; patchGenNum++)
			{
				printf("About to call GeneratePatch\n");
				steps[patchGenNum] = pg[patchGenNum]->GeneratePatch(seeds[patchGenNum],(*patches)[i+patchGenNum],boundary[patchGenNum],i+patchGenNum,false);
				(*patches)[i+patchGenNum].radius = steps[patchGenNum]/2;
			}
		
			for(int patchGenNum = 0; patchGenNum<seeds.size(); patchGenNum++)
				printf("Patch %d had %d growth steps\n",i+patchGenNum,steps[patchGenNum]);

			if (i==0)
			{
				if (steps[0] < MIN_PATCH_ITERS)
				{
					printf("Not enough growth steps (%d) on first seed\n",steps);
					exit(1);
				}
				
				printf("Adding patch to bigpatch\n");
				AddToBigPatch(bp,(*patches)[i],i);
				printf("Adding boundary to bigpatch\n");
				AddToBigPatch(bpb,boundary[0],i);	

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
			else for(int patchGenNum = 0; patchGenNum<seeds.size(); patchGenNum++)
			if (steps[patchGenNum] >= MIN_PATCH_ITERS)
			{
				for(int alignAttempts = 0; alignAttempts<2; alignAttempts++)
				{
					Aligner *al = new Aligner();
				
					std::vector<alignment> alignments;
				
					al->AlignPatches(bp,(*patches)[i+patchGenNum],alignments);
				
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
							if (am->count(i+patchGenNum)==0)
								(*am)[i+patchGenNum] = std::vector<alignment>();
							(*am)[i+patchGenNum].push_back(a);
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
						ErasePoints(bpb,(*patches)[i+patchGenNum],0,CURRENT_BOUNDARY_ERASE_DISTANCE);
						ErasePoints(bp,boundary[patchGenNum],1,NEW_BOUNDARY_ERASE_DISTANCE);

						if (!boundary[patchGenNum].Empty())
							AddToBigPatch(bpb,boundary[patchGenNum],i+patchGenNum);
						AddToBigPatch(bp,(*patches)[i+patchGenNum],i+patchGenNum);

						break;
					}
					else if (alignAttempts==0)
					{
						// Flip the patch and loop round for another try
						(*patches)[i+patchGenNum].Flip();
					}
					else
					{
						unalignedCount++;
						patches->erase(i+patchGenNum);
					}
				}
			}
			else
			{
				printf("Not enough growth steps\n");
				patches->erase(i+patchGenNum);
			}
		
			i += seeds.size();
		}
		else
		{
			printf("::::: ENDING - No seeds generated :::::\n");
			break;
		}
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
	for(int i = 0; i<NUM_THREADS; i++)
		delete pg[i];
	printf("Deleting patches\n");
	
	return true;
}

void LoadPatchesAndRelationships(std::map<int,Patch> *patches, 	AlignmentMap *am, int limit = -1, std::set<int> *restricted = NULL)
{
	DIR *dir;
	struct dirent *ent;

	// iterate through all patch files
	if ((dir = opendir (OUTPUT_DIR "/patches")) != NULL)
	{
		int i = 0;
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
			
				if ( (patchNum<=limit || limit==-1) && (restricted==NULL || restricted->count(patchNum)!=0)) 
				{
					(*patches)[patchNum]=Patch();
					
					if (i++%100==0)	
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
		
		/*
		for(auto &a : *am)
		{
			printf("%d\n",a.first);
			for(auto &al : a.second)
			{
				printf(".%d\n",std::get<0>(al));
			}
		}
		*/
	}
	
}

void LoadBadPatches(std::set<int> &badPatches, std::set<std::pair<int,int>> &manualBadRel, bool includeBadBridges = false)
{
		{
			int i;
			
			std::ifstream is(OUTPUT_DIR "/badpatches.csv");
			while(is>>i)
			{
				badPatches.insert(i);
			}
		}

		{
			int i;
			
			std::ifstream is(OUTPUT_DIR "/manualBadPatch.csv");
			while(is>>i)
			{
				badPatches.insert(i);
			}
		}

		if (includeBadBridges)
		{
			int i;
			
			std::ifstream is(OUTPUT_DIR "/badbridges.csv");
			while(is>>i)
			{
				badPatches.insert(i);
			}
		}
		
		{
			std::ifstream is(OUTPUT_DIR "/manualBadRel.csv");
			std::string line;
			while(std::getline(is,line))
			{
				std::istringstream ss(line);
				int a, b;
				char comma;
				if (ss >> a >> comma >> b)
					manualBadRel.insert({a, b});
			}
		}

}

int main(int argc, char *argv[])
{
	std::string mode("g");
	if (false) // TODO parameter checking
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
			mode = std::string(argv[1]);
	}
	
	printf("Started\n");
    fflush(stdout);

	srand(RANDOM_SEED);

	// examine alignment of two patches
	if (mode=="x")
	{
		if (argc!=6)
		{
			printf("x <patch0> <patch1> <flip 0 or 1> <seed>\n");
			exit(-1);
		}
		
		srand(atoi(argv[5]));
		
		Patch p0,p1;
		
		int patchNum0 = atoi(argv[2]);
		int patchNum1 = atoi(argv[3]);

		printf("Loading %d\n",patchNum0);		
		p0.Read(OUTPUT_DIR "/patches",patchNum0);
		printf("Loading %d\n",patchNum1);		
		p1.Read(OUTPUT_DIR "/patches",patchNum1);

		if (atoi(argv[4])==1)
			p1.Flip();
		
		Aligner *al = new Aligner();
			
		std::vector<alignment> alignments;
			
		al->AlignPatches(p0,p1,alignments);
		
		delete al;
			
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
				printf("Success\n");
			}
			else
			{
				printf("Fail\n");
			}
		}			
	}
	
	if (mode=="g")
	{
		int numPatches = 100;
		if (argc>=3)
			numPatches = atoi(argv[2]);

		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		GeneratePatches(patches,am,numPatches);
		printf("Generated patches\n");
		
		delete patches;
		delete am;
	}

	if (mode=="r")
	{
		int numPatches = 100;
		if (argc>=3)
			numPatches = atoi(argv[2]);

		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);

		GeneratePatches(patches,am,numPatches);
		printf("Generated patches\n");
		
		delete patches;
		delete am;
	}

	if (mode=="l")
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);

		{
			ofstream os(OUTPUT_DIR "/patchVolCoords.csv");
		
			for(auto &p : *patches)
			{
				Vec3 v;
				if (p.second.CentreVolCoords(v))
				{
					os << p.first << "," << v.x << "," << v.y << "," << v.z << std::endl;
				}
				else
				{
					printf("Unable to get vol coords for patch %d\n",p.first);
				}
			}
		}
		
		printf("Finished, cleaning up...\n");
		delete patches;
		delete am;
	}
	
	if (mode=="b")
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am);

		std::set<int> badPatches;
		std::vector<std::tuple<int,int,float>> badPatchScores;
	
		BadPatchFinder *bpf = new BadPatchFinder();
		printf("Finding bad patches...\n");
		{ extern long offGridTotal; offGridTotal = 0; }
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

	if (mode=="c")
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);
		AugmentAlignmentMap(*am);
		
		std::set<int> badPatches;
		std::vector<std::tuple<int,int,float>> badPatchScores;
	
		BadPatchFinder *bpf = new BadPatchFinder();
		printf("Finding bad patches...\n");
		
		bpf->FindBadPatchesGeneral(*am,patches,2,badPatches,badPatchScores);		
		
		std::set<int> round1BadPatches = badPatches;
		
		bpf->FindBadPatchesGeneral(*am,patches,3,badPatches,badPatchScores);		

		std::set<int> round2BadPatches = badPatches;
		std::set<int> round2OnlyBadPatches;
		
		std::set_difference(badPatches.begin(), badPatches.end(), round1BadPatches.begin(), round1BadPatches.end(),
                        std::inserter(round2OnlyBadPatches, round2OnlyBadPatches.begin()));
		
		bpf->FindBadPatchesGeneral(*am,patches,4,badPatches,badPatchScores);		

		std::set<int> round3BadPatches = badPatches;
		std::set<int> round3OnlyBadPatches;
		
		std::set_difference(badPatches.begin(), badPatches.end(), round2BadPatches.begin(), round2BadPatches.end(),
                        std::inserter(round3OnlyBadPatches, round3OnlyBadPatches.begin()));

		bpf->FindBadPatchesGeneral(*am,patches,5,badPatches,badPatchScores);		
						
		std::set<int> round4BadPatches = badPatches;
		std::set<int> round4OnlyBadPatches;
		
		std::set_difference(badPatches.begin(), badPatches.end(), round3BadPatches.begin(), round3BadPatches.end(),
                        std::inserter(round4OnlyBadPatches, round4OnlyBadPatches.begin()));
						
		{ extern long offGridTotal;
		  printf("points skipped as off-grid: %ld\n", offGridTotal); }
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

		printf("Round 3 bad patches\n");
		for(auto i : round3OnlyBadPatches)
		{
			printf("%d\n",i);
		}

		printf("Round 4 bad patches\n");
		for(auto i : round4OnlyBadPatches)
		{
			printf("%d\n",i);
		}

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

		
		delete bpf;
		printf("Finished, cleaning up...\n");
		delete patches;
		delete am;
	}
	
	if (mode=="v")
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);
		
		AugmentAlignmentMap(*am);

		std::set<int> badPatches;
		std::set<std::pair<int,int>> manualBadRel;

		LoadBadPatches(badPatches,manualBadRel);
		
		std::vector<int> patchOrder;
		std::vector<std::pair<int,alignment>> alignmentOrder;
		std::map<int,affineTx> patchPositions;
		std::map<int,std::set<int> > neighbourList;
		
		// make a version of this that outputs the N biggest items
		// MakeVisitOrders : it will output alignmentorder_1.txt, neighbours_1.csv etc...
		// then similarly for patchstrings, and the f command. Each will process several.
		// Simulated annealing will do the same, and the score will be based on all collections of patches...
		MakeVisitOrder(am,patches,badPatches,manualBadRel,patchOrder,alignmentOrder,patchPositions,neighbourList,true);
		
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

		{
			ofstream os(OUTPUT_DIR "/neighbours.csv");
			
			for(auto &a : alignmentOrder)
			{
				os << a.first << ","
				   << std::get<0>(a.second)
				   << ",1" << endl;
			}
		}

		{
			printf("Looking for implausible bridges\n");
			BadPatchFinder *bpf = new BadPatchFinder();
			std::set<int> badBridges;
			
			while(true)
			{
				std::map<int,int> newBadBridges;
				bpf->FindNeighbourProblems(neighbourList,patches,badBridges,newBadBridges,patchOrder,patchPositions);
				
				if (newBadBridges.size()>0)
				{
					int max = -1;
					int maxp = -1;
					
					for(auto &bb : newBadBridges)
					{
						if (bb.second>max)
						{
							max = bb.second;
							maxp = bb.first;
						}
					}
					
					printf("Bad bridge: %d\n",maxp);
					badBridges.insert(maxp);
				}
				else
					break;
			}

			{
				// remember to copy this to badbridges.csv
				std::ofstream os(OUTPUT_DIR "/badbridges_out.csv");
				for(auto i : badBridges)
				{
					os << i << std::endl;;
				}
			}
			
			delete bpf;
		}
		
		delete patches;
		delete am;
	}

	if (mode=="vm")
	{
		int numComponents = 1;
		
		if (argc>=3)
			numComponents = atoi(argv[2]);

		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);
		
		AugmentAlignmentMap(*am);

		std::set<int> badPatches;
		std::set<std::pair<int,int>> manualBadRel;

		LoadBadPatches(badPatches,manualBadRel);
		
		std::vector<std::vector<int>> patchOrders;
		std::vector<std::vector<std::pair<int,alignment>>> alignmentOrders;
		std::vector<std::map<int,affineTx>> patchPositionss;
		std::map<int,std::set<int> > neighbourList;
		
		numComponents = MakeVisitOrders(numComponents,am,patches,badPatches,manualBadRel,patchOrders,alignmentOrders,patchPositionss,neighbourList,true);
		
		{
			ofstream os(OUTPUT_DIR "/alignmentorders.txt");
			
			for(auto &i : alignmentOrders)
			{
				os << "NEW" << std::endl;
				for(auto &a : i)
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
		}

		{
			ofstream os(OUTPUT_DIR "/neighbourss.csv");
			
			for(auto &i : alignmentOrders)
			{
				os << "NEW" << std::endl;
				for(auto &a : i)
				{
					os << a.first << ","
					   << std::get<0>(a.second)
					   << ",1" << endl;
				}
			}
		}

		{
			// remember to copy this to badbridges.csv
			std::ofstream os(OUTPUT_DIR "/badbridgess_out.csv");

			for(int i = 0; i<numComponents; i++)
			{
				printf("Looking for implausible bridges in component %d\n",i);
				BadPatchFinder *bpf = new BadPatchFinder();
				std::set<int> badBridges;
				
				while(true)
				{
					std::map<int,int> newBadBridges;
					bpf->FindNeighbourProblems(neighbourList,patches,badBridges,newBadBridges,patchOrders[i],patchPositionss[i]);
					
					if (newBadBridges.size()>0)
					{
						int max = -1;
						int maxp = -1;
						
						for(auto &bb : newBadBridges)
						{
							if (bb.second>max)
							{
								max = bb.second;
								maxp = bb.first;
							}
						}
						
						printf("Bad bridge: %d\n",maxp);
						badBridges.insert(maxp);
					}
					else
						break;
				}

				os << "NEW" << std::endl;
				
				for(auto i : badBridges)
				{
					os << i << std::endl;;
				}
				
				delete bpf;
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

	if (mode=="f")
	{
		int maxDistanceThresh = -1;
		
		if (argc>=3)
			maxDistanceThresh = atoi(argv[2]);
		
		std::set<int> patchesToColour;
		
		if (argc>3)
		{
			for(int i = 3; i<argc; i++)
				patchesToColour.insert(atoi(argv[i]));
		}

		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);
		
		std::vector<int> patchOrder;
		
		{
			std::ifstream is(OUTPUT_DIR "/patchorder.csv");
			int i;
			while(is>>i)
			{
				patchOrder.push_back(i);
			}
		}

		std::set<std::pair<int,int>> manualGoodRel;

		{
			std::ifstream is(OUTPUT_DIR "/manualGoodRel.csv");
			std::string line;
			while(std::getline(is,line))
			{
				std::istringstream ss(line);
				int a, b;
				char comma;
				if (ss >> a >> comma >> b)
					manualGoodRel.insert({a, b});
			}
		}

		std::set<int> patchesInvolved;

		while(true)
		{
			std::string s;
			std::cout << "Enter q to quit, c to set patches to colour, anything else for next iteration" << std::endl;
			std::cin >> s;
			
			if (s==std::string("q"))
				break;
			
			if (s==std::string("c"))
			{
				patchesToColour.clear();
				
				for(auto p : patchesInvolved)
					patchesToColour.insert(p);
			}
			
			patchesInvolved.clear();

			std::unordered_map<int,std::tuple<float,float,float>> patchPositionsXYA;

			{
				for (auto i : patchOrder)
					(*patches)[i].UnsetPosition();

				// This must come from patchsprings.py
				// TODO - patchsprings will be rewritten in C++ soon
				ifstream is(OUTPUT_DIR "/patchPositions.txt");
				
				while(true)
				{
					int patchNum;
					float x,y,angle;
					if (is >> patchNum >> x >> y >> angle)
						patchPositionsXYA[patchNum]=std::tuple<float,float,float>(x,y,angle);
					else
						break;
				}
			}
			
			float score = ScorePlacement(am, patches, patchPositionsXYA,patchOrder, patchesToColour, manualGoodRel, patchesInvolved, maxDistanceThresh, 1.0, true, true, false);
			
			printf("Score=%f\n",score);
		}
		
		delete patches;
		delete am;
	}

	if (mode=="fm")
	{
		int maxDistanceThresh = -1;
		
		if (argc>=3)
			maxDistanceThresh = atoi(argv[2]);

		int numComponents = 1;
		
		if (argc>=4)
			numComponents = atoi(argv[3]);
		
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);

		std::set<std::pair<int,int>> manualGoodRel;

		{
			std::ifstream is(OUTPUT_DIR "/manualGoodRel.csv");
			std::string line;
			while(std::getline(is,line))
			{
				std::istringstream ss(line);
				int a, b;
				char comma;
				if (ss >> a >> comma >> b)
					manualGoodRel.insert({a, b});
			}
		}
		
		for(int compIndex = 0; compIndex < numComponents; compIndex++)
		{
			std::vector<int> patchOrder;
			
			{
				std::ifstream is(OUTPUT_DIR "/patchorders.csv");
				int poCounter = 0;
				std::string line;
				while (std::getline(is, line)) {
					if (line=="NEW")
					{
						printf("Encountered NEW reading patchOrder\n");
						if (poCounter>compIndex)
							break;
						else
						{
							patchOrder.clear();
							poCounter++;
						}
					}
					else
					{
						patchOrder.push_back(atoi(line.c_str()));
					}
				}

			}

			std::set<int> patchesInvolved;


			std::unordered_map<int,std::tuple<float,float,float>> patchPositionsXYA;

			{
				for (auto i : patchOrder)
					(*patches)[i].UnsetPosition();

				ostringstream oss;
				oss << OUTPUT_DIR << "/patchPositions_" << compIndex << ".txt";
				ifstream is(oss.str());
					
				while(true)
				{
					int patchNum;
					float x,y,angle;
					if (is >> patchNum >> x >> y >> angle)
						patchPositionsXYA[patchNum]=std::tuple<float,float,float>(x,y,angle);
					else
						break;
				}
			}

			std::set<int> patchesToColour;
			float score = ScorePlacement(am, patches, patchPositionsXYA,patchOrder, patchesToColour, manualGoodRel, patchesInvolved, maxDistanceThresh, 1.0, true, true, false, compIndex);
				
			printf("Score=%f\n",score);
		}
		
		delete patches;
		delete am;
	}

    // 'a' and 'A' generate images in sliceanim - moving up and down the scroll as more and more patches are added.
    // This helps to spot mistakes.	
	// after generating sliceanim, turn it into an mp4 using this
	// ffmpeg -framerate 24 -i d:/pipelineOutput/sliceanim/s_%08d.tif -vf scale=iw/2:ih/2 -c:v libx264 -pix_fmt yuv420p d:/pipelineOutput/sliceanim.mp4
	// A means show global coords of patches
	if (mode=="a" || mode=="A")
	{
		int closeUpIter = -1;
		
		if (argc==3)
			closeUpIter = atoi(argv[2]);
		
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);

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

		
		// Enough buffers that we can do several layers before reloading buffers,
		ZARR_1_b700 *surfaceZarr = ZARROpen_1_b700(SURFACE_ZARR);

		printf("Rendering...\n");

		SliceAnimRender(surfaceZarr,std::string(OUTPUT_DIR "/sliceanim"),100,50,1,closeUpIter,patches,patchOrder,mode=="A");
	
		ZARRClose_1_b700(surfaceZarr);
	}

	if (mode=="p")
	{
		std::vector<int> patchesToShow;
		std::set<int> patchesToShowSet;
		
		for(int i = 2; i<argc; i++)
		{
			patchesToShow.push_back(atoi(argv[i]));
			patchesToShowSet.insert(atoi(argv[i]));
		}
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT,&patchesToShowSet);
		printf("Finished loading\n");
		// Enough buffers that we can do several layers before reloading buffers,
		ZARR_1_b700 *surfaceZarr = ZARROpen_1_b700(SURFACE_ZARR);

		printf("Rendering...\n");

		SliceAnimRender(surfaceZarr,std::string(OUTPUT_DIR "/sliceprobe"),patchesToShow.size(),20,1,-1,patches,patchesToShow);
		writePatchColourKey(patchesToShow,OUTPUT_DIR "/sliceprobe/key.tif");
		
		ZARRClose_1_b700(surfaceZarr);
	}

	if (mode=="s")
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

	// q <path> <patchnum> x y
	// returns the vx,vy,vz volume coords of x,y
	if (mode=="q")
	{		
		int patchNum, x, y;
		
		if (argc==6)
		{
			patchNum = atoi(argv[3]);
			x = atoi(argv[4]);
			y = atoi(argv[5]);
		}
		else
		{
			printf("%s q <path> <patchnum> x y\n",argv[0]);
			printf("Show the vx,vy,vz coords of 0-based x,y (e.g. from tif image)\n");
			exit(-1);
		}

		Patch p;
		
		p.Read(argv[2],patchNum);
		
		if (x<p.maxux-p.minux && y<p.maxuy-p.minuy)
		{			
			if (p.pointGrid[x][y])
			{
				printf("%f,%f,%f\n",p.pointGrid[x][y]->v.x,p.pointGrid[x][y]->v.y,p.pointGrid[x][y]->v.z);
			}
			else
				printf("No point found\n");
			
		}
	}

	if (mode=="n")
	{
		int iterations = 100;
		float initT0 = -1;
		
		if (argc>2)
			iterations = atoi(argv[2]);
        if (argc>3)
			initT0 = atof(argv[3]);

		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);

		AugmentAlignmentMap(*am);

		std::vector<int> patchNums;
		for(auto &i : *patches)
			patchNums.push_back(i.first);

		std::set<int> badPatches;
		std::set<std::pair<int,int>> manualBadRel;

		LoadBadPatches(badPatches,manualBadRel,false);
		
		std::set<int> badBridges;
		{
			int i;
			std::ifstream is(OUTPUT_DIR "/badbridges.csv");
			while(is>>i)
			{
				badBridges.insert(i);
			}
		}

		printf("Annealing\n");
		Anneal(am,patches,patchNums,badPatches,manualBadRel,badBridges,iterations,4,initT0);
		
		delete patches;
		delete am;
	}

	if (mode=="nm")
	{
		int numComponents = 1;
		
		if (argc>=3)
			numComponents = atoi(argv[2]);

		int iterations = 100;
		float initT0 = -1;
		
		if (argc>=4)
			iterations = atoi(argv[3]);
        if (argc>=5)
			initT0 = atof(argv[4]);

		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);

		AugmentAlignmentMap(*am);

		std::vector<int> patchNums;
		for(auto &i : *patches)
			patchNums.push_back(i.first);

		std::set<int> badPatches;
		std::set<std::pair<int,int>> manualBadRel;

		LoadBadPatches(badPatches,manualBadRel,false);
		
		std::set<int> badBridges;
		{
			int i;
			std::ifstream is(OUTPUT_DIR "/badbridges.csv");
			while(is>>i)
			{
				badBridges.insert(i);
			}
		}

		printf("Annealing\n");
		AnnealAll(numComponents,am,patches,patchNums,badPatches,manualBadRel,badBridges,iterations,4,initT0);
		
		delete patches;
		delete am;
	}

	if (mode=="o")
	{
		AlignmentMap *am = new AlignmentMap;
		std::map<int,Patch> *patches = new std::map<int,Patch>;

		printf("Loading patches and relationships...\n");
		LoadPatchesAndRelationships(patches,am,PATCH_LIMIT);

		AugmentAlignmentMap(*am);

		std::vector<int> patchNums;
		for(auto &i : *patches)
			patchNums.push_back(i.first);

		std::set<int> badPatches;
		std::set<std::pair<int,int>> manualBadRel;

		LoadBadPatches(badPatches,manualBadRel,false);
		
		std::set<int> badBridges;
		{
			int i;
			std::ifstream is(OUTPUT_DIR "/badbridges.csv");
			while(is>>i)
			{
				badBridges.insert(i);
			}
		}

		printf("Omission testing\n");
		OmissionTest(am,patches,patchNums,badPatches,manualBadRel,badBridges,4);
		
		delete patches;
		delete am;
	}

	if (mode=="h")
	{
	    printf("Running patchsprings...\n");
		{
			PatchSpringSimulation pss(QUADMESH_SIZE,OUTPUT_DIR);
			
			pss.loadPatchVolCoords(OUTPUT_DIR "/patchVolCoords.csv");
			
			std::vector<std::vector<std::string>> alignmentOrderDash;
			{
				std::ifstream f(OUTPUT_DIR "/alignmentorder.txt");
				if (!f) {
					std::cerr << "Could not open alignmentorder.txt\n";
				}
				std::string line;
				while (std::getline(f, line)) {
					alignmentOrderDash.push_back(splitOnSpaceDropLast(line));
				}
			}

			printf("Loading patches for patchsprings...\n");
			pss.loadPatches(alignmentOrderDash, PATCH_LIMIT);
			
			printf("Running patchsprings...\n");
		    pss.run(50);
			printf("Finished patchsprings...\n");

		}
	    printf("Finished running patchsprings\n");
	}

	if (mode=="hm")
	{
		int numComponents = 1;
		
		if (argc>=3)
			numComponents = atoi(argv[2]);

		for(int i = 0; i<numComponents; i++)
		{
			printf("Patchsprings for component %d\n",i);
			
			PatchSpringSimulation pss(QUADMESH_SIZE,OUTPUT_DIR,i);
			
			pss.loadPatchVolCoords(OUTPUT_DIR "/patchVolCoords.csv");
			
			std::vector<std::vector<std::string>> alignmentOrderDash;
			{
				int alCounter = 0;
				
				std::ifstream f(OUTPUT_DIR "/alignmentorders.txt");
				if (!f) {
					std::cerr << "Could not open alignmentorders.txt\n";
				}
				std::string line;
				while (std::getline(f, line)) {
					if (line=="NEW")
					{
						printf("Encountered NEW reading alignmentOrder\n");
						if (alCounter>i)
							break;
						else
						{
							alignmentOrderDash.clear();
							alCounter++;
						}
					}
					else
					{
						alignmentOrderDash.push_back(splitOnSpaceDropLast(line));
					}
				}
			}

			// Check whether we are past max number of components
			if (alignmentOrderDash.size()==0)
				break;
			
			printf("Loading patches for patchsprings...\n");
			pss.loadPatches(alignmentOrderDash, PATCH_LIMIT);
			
			printf("Running patchsprings...\n");
		    pss.run(50);
			printf("Finished patchsprings...\n");

		}
	    printf("Finished running patchsprings\n");
	}

	// parameters : patch name, output prefix
	if (mode=="z")
	{
		if (argc != 5)
		{
			printf("z <patch> <output-prefix> <max-dist>\n");
			printf("Produce parallel patches\n");
		}

	    printf("Producing parallel patches...\n");
		
		float maxDist = atof(argv[4]);
		std::string patchName(argv[2]);
		
		Patch patchIn;
		
		patchIn.Read(argv[2],0);
		
		int i = 0;
		for(float d = -maxDist; d<=maxDist; d+=1.0,i++)
		{
			printf("d=%f\n",d);
			
			Patch patchOut;
			
			patchIn.CreateParallelPatch(d,patchOut);
			
			std::ostringstream outName;
			outName << argv[3];
						
			patchOut.Write(outName.str(),i);
		}
	    printf("Finished producing parallel patches\n");
	}
	
	printf("Done\n");
	exit(0);
}

