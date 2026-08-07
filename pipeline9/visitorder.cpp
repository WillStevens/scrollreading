#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>

#include <stdio.h>

#include "parameters.h"
#include "position_patches.h"

struct ComponentInfo {
    int size;
    int minVertex;
};

std::vector<ComponentInfo> getComponents(const std::map<int, std::set<int>>& neighbourList)
{
    std::vector<ComponentInfo> components;
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

		std::vector<ComponentInfo> components = getComponents(neighbourList);
		
		if (showSize)
		{
			printf("Size and min vertex of components\n");
			for(auto &ci : components)
			{
				printf("%d : %d\n",ci.size,ci.minVertex);
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
			std::ofstream os(OUTPUT_DIR "/patchorder.csv");
			for (auto i : patchOrder)
			{
				os << i << std::endl;
			}
		}
		
		//printf("Producing alignment order...\n");

		PositionPatches(patches,*am,patchOrder,patchPositions,alignmentOrder,manualBadRel);
}
