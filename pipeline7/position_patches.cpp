#include "position_patches.h"

// Add the reversals too (keep the variances the same as the original)
void AugmentAlignmentMap(AlignmentMap &alignmentMap)
{
	AlignmentMap newEntries;
	
	for(auto &a : alignmentMap)
	{
		for(auto &al : a.second)
		{
			int p = std::get<0>(al);
			
			if (newEntries.count(p)==0)
			{
				newEntries[p] = std::vector<alignment>();
			}
			
			affineTx inv = AffineTxInverse(
			  affineTx(
			    std::get<7>(al),
			    std::get<8>(al),
			    std::get<9>(al),
			    std::get<10>(al),
			    std::get<11>(al),
			    std::get<12>(al)
			  ));
			
			newEntries[p].push_back(alignment(
				a.first,
				std::get<1>(al),
				std::get<2>(al),
				std::get<3>(al),
				std::get<4>(al),
				std::get<5>(al),
				std::get<6>(al),
				std::get<0>(inv),
				std::get<1>(inv),
				std::get<2>(inv),
				std::get<3>(inv),
				std::get<4>(inv),
				std::get<5>(inv)
				));
		}
	}
	
	for(auto &a : newEntries)
	{
		if (alignmentMap.count(a.first)==0)
		{
			alignmentMap[a.first] = a.second;
		}
		else
		{
			alignmentMap[a.first].insert(alignmentMap[a.first].end(),a.second.begin(),a.second.end());
		}
	}
}

// Given patches and alignments, work out global positions. Also output the order in which the patches were aligned and positioned
void PositionPatches(std::map<int,Patch> *patches, AlignmentMap &alignmentMap, std::vector<int> &patchOrder, std::map<int,affineTx> &patchPositions, std::vector<std::pair<int,alignment>> &alignmentOrder)
{
	int lastPlaced = -1;
	for(auto i : patchOrder)
	{
		if (lastPlaced==-1)
		{
			patchPositions[i] = affineTx(1,0,0,0,1,0);
			alignmentOrder.push_back(std::pair<int,alignment>(0,alignment(-1,0.0,0.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0,0.0)));
		}
		else
		{
			if (alignmentMap.count(i) != 0)
			{
				printf("Found %d in alignmentMap\n",i);
				for(auto &aln : alignmentMap[i])
				{
					int p = std::get<0>(aln);

					printf("Looking for %d in patchPositions\n",p);
					
					if (patchPositions.count(p) != 0)
					{
						printf("Found %d in alignment list for %d\n",p,i);

						alignmentOrder.push_back(std::pair<int,alignment>(i,aln));
						
						affineTx pPos = patchPositions[p];
						affineTx iAlign = affineTx(
							std::get<7>(aln),
							std::get<8>(aln),
							std::get<9>(aln),
							std::get<10>(aln),
							std::get<11>(aln),
							std::get<12>(aln));
							
						patchPositions[i] = AffineTxMultiply(pPos,iAlign);
					}
				}
			}
		}
		
		lastPlaced = i;
	}
}