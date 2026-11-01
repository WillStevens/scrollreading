#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <queue>
#include <algorithm>

#include <stdio.h>

#include "parameters.h"
#include "position_patches.h"
#include "common_types.h"

// The pairs contain size, minVertex
std::vector<std::pair<int,int>> getComponents(const std::map<int, std::set<int>>& neighbourList)
{
    std::vector<std::pair<int,int>> components;
    std::set<int> globalVisited;

    for (auto& [vertex, neighbours] : neighbourList)
    {
        if (globalVisited.count(vertex)) continue;

        // BFS over this component
        std::set<int> visited;
        std::queue<int> toVisit;

        toVisit.push(vertex);
        visited.insert(vertex);

        while (!toVisit.empty())
        {
            int current = toVisit.front();
            toVisit.pop();

            for (int neighbour : neighbourList.at(current))
            {
                if (!visited.count(neighbour))
                {
                    visited.insert(neighbour);
                    toVisit.push(neighbour);
                }
            }
        }

        globalVisited.insert(visited.begin(), visited.end());

        components.push_back({
            (int)visited.size(),
            *visited.begin()  // set is ordered, so begin() is the minimum
        });
    }

    return components;
}

void MakeVisitOrder(AlignmentMap *am, std::map<int,Patch> *patches,std::set<int> &badPatches,std::set<std::pair<int,int>> &manualBadRel, std::vector<int> &patchOrder, std::vector<std::pair<int,alignment>> &alignmentOrder,std::map<int,affineTx> &patchPositions, std::map<int,std::set<int> > &neighbourList, AlignmentScoreMap &patchAlignmentScores, bool showSize=false, bool saveOutput = true)
{
		int minPatchInNeighbourList = -1;
		
		//printf("Constructing neighbour list\n");
		for(auto &a : *am)
		{
			int patch1 = a.first;
		
			if (badPatches.count(patch1)==0)
			{
				// Iterate over alignments
				for(auto al : a.second)
				{	
			        printf("Testing %d,%d\n",patch1,std::get<0>(al));
					if (std::get<13>(al)) continue;
					
					int patch2 = std::get<0>(al);

					printf("Testing for bad patch %d\n",patch2);
					if (badPatches.count(patch2)==0 && manualBadRel.count(std::pair<int,int>(patch1,patch2))==0 && manualBadRel.count(std::pair<int,int>(patch2,patch1))==0)
					{
						printf("Neighbour!\n");
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

		std::vector<std::pair<int,int>> components = getComponents(neighbourList);
		
		if (showSize)
		{
			printf("Size and min vertex of components\n");
			for(auto &ci : components)
			{
				printf("%d : %d\n",ci.first,ci.second);
			}
		}

		
		//printf("Calculating patch order...\n");
		
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


			float minAlignmentScore = -1;
			int minNeighbour;
			bool first = true;
			for(auto i : toVisitSet)
			{
				float alignmentScore = -1;
				if (patchAlignmentScores.count(i))
					alignmentScore = patchAlignmentScores[i];
	
				if (first || alignmentScore < minAlignmentScore)
				{
					minAlignmentScore = alignmentScore;
					minNeighbour = i;
				}
			
				first = false;
			}
				
			toVisitSet.erase(minNeighbour);
			currentVisit = minNeighbour;
		}

		if (saveOutput)
		{
			std::ofstream os(OUTPUT_DIR "/patchorder.csv");
			for (auto i : patchOrder)
			{
				os << i << std::endl;
			}
		}
		
		//printf("Producing alignment order...\n");

		PositionPatches(patches,*am,patchOrder,patchPositions,alignmentOrder,manualBadRel);
}

// version of MakeVisitOrder that will output data for the N largest components...
// WS 2026-10-26 - it will also output a value that can be used in a cost function: score for any two patches are much farther apart than they should be...
int MakeVisitOrders(int N, AlignmentMap *am, std::map<int,Patch> *patches,std::set<int> &badPatches,std::set<std::pair<int,int>> &manualBadRel, std::vector< std::vector<int>> &patchOrders, std::vector<std::vector<std::pair<int,alignment>>> &alignmentOrders,std::vector<std::map<int,affineTx>> &patchPositionss, std::map<int,std::set<int> > &neighbourList, AlignmentScoreMap &patchAlignmentScores, bool showSize=false, bool saveOutput = true)
{
		int minPatchInNeighbourList = -1;
		
		//printf("Constructing neighbour list\n");
		for(auto &a : *am)
		{
			int patch1 = a.first;
		
			if (badPatches.count(patch1)==0)
			{
				// Iterate over alignments
				for(auto al : a.second)
				{
					if (std::get<13>(al)) continue;
					
					int patch2 = std::get<0>(al);

					if (badPatches.count(patch2)==0 && manualBadRel.count(std::pair<int,int>(patch1,patch2))==0 && manualBadRel.count(std::pair<int,int>(patch2,patch1))==0)
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

		std::vector<std::pair<int,int>> components = getComponents(neighbourList);
	
		// sort by component size
		std::sort(components.begin(),components.end(),std::greater<>());
		
		if (showSize)
		{
			printf("Size and min vertex of components\n");
			for(auto &ci : components)
			{
				printf("%d : %d\n",ci.first,ci.second);
			}
		}

		int noOfCompToOutput = (N>(int)components.size())?(int)components.size():N;
		
		//printf("Calculating patch order...\n");

		for(int compIndex = 0; compIndex < noOfCompToOutput; compIndex++)
		{
			std::set<int> visited;
			int currentVisit = components[compIndex].second;
			std::set<int> toVisitSet;
		
			std::vector<int> patchOrder;
			
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

				float minAlignmentScore = -1;
				int minNeighbour;
				bool first = true;
				for(auto i : toVisitSet)
				{
//					float alignmentScore = i;
					float alignmentScore = -1;
					if (patchAlignmentScores.count(i))
						alignmentScore = patchAlignmentScores[i];
					else
						printf("Didn't find alignment score for %d\n",i);
	
					if (first || alignmentScore < minAlignmentScore)
					{
						minAlignmentScore = alignmentScore;
						minNeighbour = i;
					}
			
					first = false;
				}
				toVisitSet.erase(minNeighbour);
				currentVisit = minNeighbour;
			}

			patchOrders.push_back(patchOrder);
			
			if (saveOutput)
			{
				std::ofstream os(OUTPUT_DIR "/patchorder.csv");
				for (auto i : patchOrder)
				{
					os << i << std::endl;
				}
			}
			
			std::vector<std::pair<int,alignment>> alignmentOrder;
			std::map<int,affineTx> patchPositions;
			
			PositionPatches(patches,*am,patchOrder,patchPositions,alignmentOrder,manualBadRel);
			
			alignmentOrders.push_back(alignmentOrder);
			patchPositionss.push_back(patchPositions);

		}

		if (saveOutput)
		{
			std::ofstream os(OUTPUT_DIR "/patchorders.csv");
			for (auto &i : patchOrders)
			{
				os << "NEW" << endl;
				for(auto j : i)
					os << j << std::endl;
			}
		}
		
	return noOfCompToOutput;
}
