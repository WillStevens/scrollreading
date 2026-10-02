#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>

#include <stdio.h>

#include "parameters.h"
#include "position_patches.h"


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

void MakeVisitOrder(AlignmentMap *am, std::map<int,Patch> *patches,std::set<int> &badPatches,std::set<std::pair<int,int>> &manualBadRel, std::vector<int> &patchOrder, std::vector<std::pair<int,alignment>> &alignmentOrder,std::map<int,affineTx> &patchPositions, std::map<int,std::set<int> > &neighbourList, bool showSize=false, bool saveOutput = true)
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
			
			auto nh = toVisitSet.extract(toVisitSet.begin());
			currentVisit = nh.value();
		}

		if (saveOutput)
		{
			std::ofstream os(outPath("/patchorder.csv"));
			for (auto i : patchOrder)
			{
				os << i << std::endl;
			}
		}
		
		//printf("Producing alignment order...\n");

		PositionPatches(patches,*am,patchOrder,patchPositions,alignmentOrder,manualBadRel);
}

// version of MakeVisitOrder that will output data for the N largest components...
int MakeVisitOrders(int N, AlignmentMap *am, std::map<int,Patch> *patches,std::set<int> &badPatches,std::set<std::pair<int,int>> &manualBadRel, std::vector< std::vector<int>> &patchOrders, std::vector<std::vector<std::pair<int,alignment>>> &alignmentOrders,std::vector<std::map<int,affineTx>> &patchPositionss, std::map<int,std::set<int> > &neighbourList, bool showSize=false, bool saveOutput = true)
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

		int noOfCompToOutput = (N>components.size())?components.size():N;
		
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
			
				auto nh = toVisitSet.extract(toVisitSet.begin());
				currentVisit = nh.value();
			}

			patchOrders.push_back(patchOrder);
			
			if (saveOutput)
			{
				std::ofstream os(outPath("/patchorder.csv"));
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
			std::ofstream os(outPath("/patchorders.csv"));
			for (auto &i : patchOrders)
			{
				os << "NEW" << endl;
				for(auto j : i)
					os << j << std::endl;
			}
		}
		
	return noOfCompToOutput;
}
