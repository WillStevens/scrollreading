#include <iostream>
#include <fstream>
#include <set>
#include <unordered_map>
#include <map>
#include <vector>
#include <queue>
#include <sstream>
#include <random>
#include <algorithm>

#include <stdio.h>

#include <omp.h>

#include "visitorder.h"
#include "scoreplacement.h"
#include "parameters.h"
#include "PatchSpringSimulation.hpp"

// Runs the full pipeline for a candidate state: builds effective bad patches,
// computes visit order, writes alignment order, runs the spring simulation,
// scores the placement, and updates patch heat from the mismatches found.
// Returns the score. This is the single expensive operation in the whole
// algorithm — both calibration sampling and the main loop route through here.
float EvaluateStateOT(AlignmentMap *am, std::map<int,Patch> *patches,
                     std::set<int> &state, std::set<int> &badPatches,
                     std::set<std::pair<int,int>> &manualBadRel,
                     std::set<int> &patchesInvolved)
{
	std::set<int> effectiveBadPatches;

	std::set_union(badPatches.begin(), badPatches.end(),
	               state.begin(), state.end(),
	               std::inserter(effectiveBadPatches, effectiveBadPatches.begin()));

	std::vector<int> patchOrder;
	std::vector<std::pair<int,alignment>> alignmentOrder;
	std::vector<std::vector<std::string>> alignmentOrderDash;
	std::map<int,affineTx> patchPositions;
	std::map<int,std::set<int> > neighbourList;

	MakeVisitOrder(am, patches, effectiveBadPatches, manualBadRel,
	               patchOrder, alignmentOrder, patchPositions, neighbourList,false,false);

				   
	{
		for(auto &a : alignmentOrder)
		{
			std::vector<std::string> row;
			auto toStr = [](auto val)
			{
				std::ostringstream oss;
				oss << val;
				return oss.str();
			};

			row.push_back(toStr(a.first));
			row.push_back(toStr((*patches)[a.first].radius));
			row.push_back(toStr(std::get<0>(a.second)));
			row.push_back(toStr(std::get<7>(a.second)));
			row.push_back(toStr(std::get<8>(a.second)));
			row.push_back(toStr(std::get<9>(a.second)));
			row.push_back(toStr(std::get<10>(a.second)));
			row.push_back(toStr(std::get<11>(a.second)));
			row.push_back(toStr(std::get<12>(a.second)));

			alignmentOrderDash.push_back(std::move(row));
		}
	}

	std::unordered_map<int,std::tuple<float,float,float>> patchPositionsXYA;
	
	//printf("Running patchsprings...\n");
	{
		PatchSpringSimulation pss(QUADMESH_SIZE,outputDir(),false);

		pss.loadPatchVolCoords(outPath("/patchVolCoords.csv"));

		//printf("Loading patches for patchsprings...\n");
		pss.loadPatches(alignmentOrderDash, patches->size());

		//printf("Running patchsprings...\n");
		pss.run(50);
		//printf("Finished patchsprings...\n");
	
	    //printf("Finished running patchsprings\n");


	    // Get patch positions from pss and set them in *patches
		for (auto i : patchOrder)
		{
			(*patches)[i].UnsetPosition();
			float x,y,angle;
            pss.GetPatchPosition(i,x,y,angle);
			
			patchPositionsXYA[i]=std::tuple<float,float,float>(x,y,angle);
			
			//printf("%d %f %f %f\n",i,x,y,angle);
		}
	}
	
    std::set<int> patchesToColour;
    std::set<std::pair<int,int>> manualGoodRel;
	
	float score = ScorePlacementAreaAndIncon(am, patches, patchPositionsXYA,patchOrder, patchesToColour,
	                              manualGoodRel, patchesInvolved, 30, 10, false, false);

	return score;
}



void OmissionTest(AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums,
            std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel,
            std::set<int> badBridges, int N)
{	
    std::vector<std::tuple<float,int>> patchScores;

	for(int i=0; i<(int)patchNums.size(); i+=N)
	{
		printf("Patch %d\n",i);
		
		std::set<int> testState[N];
		float patchScore[N];
		
		for(int n=0; n<N && i+n<(int)patchNums.size(); n++)
			testState[n].insert(patchNums[i+n]);
           
		if (i+N<=(int)patchNums.size())
		{
			// parallelize if we can...
		
			printf("About to run parallel part\n");
			#pragma omp parallel for schedule(dynamic)
			for(int n=0;  n<N; n++)
			{
				std::set<int> localPatchesInvolved;
				patchScore[n] = EvaluateStateOT(am, patches, testState[n], badPatches,
											 manualBadRel, localPatchesInvolved);
			}
		}
		else
		{
			// else run it serially.
			for(int n=0;  n<N && i+n<(int)patchNums.size(); n++)
			{
				std::set<int> localPatchesInvolved;
				patchScore[n] = EvaluateStateOT(am, patches, testState[n], badPatches,
											 manualBadRel, localPatchesInvolved);
			}
		}

		printf("Batch:\n");
		for(int n=0; n<N && i+n<(int)patchNums.size(); n++)
		{
			printf("%d %f\n",patchNums[i+n],patchScore[n]);
			patchScores.push_back({patchScore[n],patchNums[i+n]});
		}
	}		
	
	std::sort(patchScores.begin(),patchScores.end());
	
	for(int i = 0; i<patchScores.size(); i++)
	{
		printf("%d %f\n",std::get<1>(patchScores[i]),std::get<0>(patchScores[i]));
	}	
}
