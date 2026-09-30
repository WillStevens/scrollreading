#include <algorithm>
#include <omp.h>

#include "badpatchfinder.h"

// A7.9: the counter of points falling outside the grid was incremented and never printed.
// It says how often the bounds check we added is actually preventing an out-of-array write:
// if it is zero, the upstream defect is not reachable on this data and that is worth stating;
// if it is not, the number is the measure of how reachable it is.
long offGridTotal = 0;
void BadPatchFinder::ClearRendered(void)
{
	offGridTotal += mainState.offGrid;
	mainState.Clear();
	maxDistance = 0.0;
	cellsOverlap = 0;
	cellsBlind = 0;
}

void BadPatchFinder::PrecomputeNormals(std::map<int,Patch> *patches)
{
	for(auto &entry : *patches)
	{
		std::vector<Vec3> &v = normals[entry.first];
		if (!v.empty()) continue;
		Patch &p = entry.second;
		for(PatchIterator pi = p.Begin(); p.Next(pi);)
		{
			Vec3 n;
			if (!p.GetNormal(pi.p->x,pi.p->y,n)) n = Vec3(0,0,0);
			v.push_back(n);
		}
	}
}

void BadPatchFinder::PlacePatchInto(RenderState &st, Patch &p1, int patchNum, const affineTx &aftx, bool first)
{
	std::unordered_map<int, std::vector<Vec3>>::const_iterator nit = normals.find(patchNum);
	const std::vector<Vec3> *cache = (nit == normals.end()) ? NULL : &nit->second;
	size_t k = 0;

	for(PatchIterator pi = p1.Begin(); p1.Next(pi);)
	{
		float x,y,px,py,pz;
		x=pi.p->x; y=pi.p->y; px=pi.p->v.x; py=pi.p->v.y; pz=pi.p->v.z;

		Vec3 normal;
		if (cache && k < cache->size())
			normal = (*cache)[k];
		else if (!p1.GetNormal(pi.p->x,pi.p->y,normal))
			normal = Vec3(0,0,0);
		k++;

		// transform to get position of this patch point
		AffineTxApply(aftx,x,y);

		int xrdi = ROUND(x/RESCALE);
		int yrdi = ROUND(y/RESCALE);

		// R_ARRAY_SIZE is documented as needing to be large enough, but nothing ever checked it.
		// A patch placed far from the origin, which happens as soon as patches from separately
		// grown runs are aligned into one set, indexes outside the grid and corrupts memory with
		// no message. Points that fall outside are skipped: they could not have been compared
		// against anything anyway, because the other patch cannot be there either.
		int gx = xrdi + R_ARRAY_SIZE/2, gy = yrdi + R_ARRAY_SIZE/2;
		if (gx < 0 || gy < 0 || gx >= R_ARRAY_SIZE || gy >= R_ARRAY_SIZE)
		{
			st.offGrid++;		// counted here, summed into offGridTotal and printed at the end of the stage
			continue;
		}

		size_t cell = (size_t)gx*R_ARRAY_SIZE + gy;
		float *c = &st.grid[cell*6];

		if (first)
		{
			if (c[0] != 0)
			{
				printf("Error - encountered more than one point per cell on first pass - expecting that aftx will be identity so that there is exactly one point per cell\n");
				exit(-1);
			}

			c[0]=px; c[1]=py; c[2]=pz;
			c[3]=normal.x; c[4]=normal.y; c[5]=normal.z;

			st.touched.push_back((int)cell);
		}
		else
		{
			if (c[0] != 0)
			{
				Vec3 pPos(px,py,pz);
				Vec3 ePos(c[0],c[1],c[2]);
				Vec3 eNormal(c[3],c[4],c[5]);

				// So long as we have at least 1 normal, we can work out a distance.
				// If a normal is missing it is set to zero, so the distance comes out as zero,
				// so if both are missing the distance will be zero.
				float distance1 = fabs(Vec3::dot(ePos-pPos,eNormal));
				float distance2 = fabs(Vec3::dot(ePos-pPos,normal));

				// Use the worst of the distances calculated
				float distance = distance1>distance2 ? distance1 : distance2;

				if (distance > st.maxDistance) st.maxDistance = distance;

				// instrumentation: how much of the overlap the test could actually see
				st.cellsOverlap++;
				if (eNormal.x==0.0f && eNormal.y==0.0f && eNormal.z==0.0f &&
				    normal.x==0.0f && normal.y==0.0f && normal.z==0.0f)
					st.cellsBlind++;
			}
		}
	}
}

void BadPatchFinder::PlacePatch(Patch &p1, int patchNum,const affineTx &aftx, bool first)
{
	PlacePatchInto(mainState,p1,patchNum,aftx,first);
	maxDistance = mainState.maxDistance;
	cellsOverlap = mainState.cellsOverlap;
	cellsBlind = mainState.cellsBlind;
}

#ifdef OUTPUT_DISTANCE_TIF
void BadPatchFinder::RenderDistances(void)
{
		TIFF *tif = TIFFOpen("distances.tif","w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, D_SIZE_X); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, D_SIZE_Y); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(D_SIZE_X);
			for(row=0; row<D_SIZE_Y;row++)
			{
				for(int i=0; i<D_SIZE_X; i++)
				{
				   if (distances[i][row]!=0)
				     ((uint8_t *)buf)[i] = ((distances[i][row]-1.0)*5)/6;
				   else
				     ((uint8_t *)buf)[i] = 255;
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

}
#endif

#ifdef COLLECT_DISTANCE_DISTRIB
void BadPatchFinder::CollectDistanceStats(void)
{
	std::list<float> distanceList;
	
	for(int row=0; row<D_SIZE_Y;row++)
	{
		for(int i=0; i<D_SIZE_X; i++)
		{
		   if (distances[i][row]!=0)
		     distanceList.push_back(distances[i][row]-1.0);
		}
	}
	
	// sort the distance list
	// work out quartiles and median
}
#endif

// Given an alignment map and the patches, iterate through all neighbouring patches
// and find patches with mismatch between 2D x,y and 3D x,y,z
void BadPatchFinder::FindBadPatches(const AlignmentMap &am, std::map<int,Patch> *patches, std::set<int> &badPatches, std::vector<std::tuple<int,int,float>> &badPatchScores)
{
	std::list<std::pair<int,int>> badPatchPairs;

	PrecomputeNormals(patches);

	// The pairs are flattened into a list, measured in parallel (each pair is independent of the
	// others), and emitted afterwards in the original order, so the output is unchanged.
	// (*patches)[p] and am[p] are deliberately not used inside the parallel loop: std::map's
	// operator[] inserts when the key is absent, which would be a silent data race on the map.
	std::vector<std::pair<int,int>> pairList;             // (patch1, index into its alignments)
	std::vector<const std::vector<alignment> *> alignOf;
	for(const auto &a : am)
		for(size_t k = 0; k<a.second.size(); k++)
		{
			pairList.push_back(std::pair<int,int>(a.first,(int)k));
			alignOf.push_back(&a.second);
		}

	const size_t numPairs = pairList.size();
	std::vector<float> pairMaxDistance(numPairs, 0.0f);

	{
		int nt = omp_get_max_threads();
		std::vector<RenderState> states(nt);

		#pragma omp parallel for schedule(dynamic,8)
		for(size_t q = 0; q<numPairs; q++)
		{
			RenderState &st = states[omp_get_thread_num()];
			st.Clear();

			int patch1 = pairList[q].first;
			const alignment &al = (*alignOf[q])[pairList[q].second];
			int patch2 = std::get<0>(al);

			affineTx aftx(std::get<7>(al),std::get<8>(al),std::get<9>(al),std::get<10>(al),std::get<11>(al),std::get<12>(al));

			PlacePatchInto(st,patches->at(patch2),patch2,affineTx(1,0,0,0,1,0),true);
			PlacePatchInto(st,patches->at(patch1),patch1,aftx,false);

			pairMaxDistance[q] = st.maxDistance;
		}
	}

	for(size_t q = 0; q<numPairs; q++)
	{
		int patch1 = pairList[q].first;
		int patch2 = std::get<0>((*alignOf[q])[pairList[q].second]);

		printf("%d,%d : %f\n",patch2,patch1,pairMaxDistance[q]);
		badPatchScores.push_back(std::tuple<int,int,float>(patch2,patch1,pairMaxDistance[q]));

		if (pairMaxDistance[q] > BP_MAX_XYZ_DISTANCE)
			badPatchPairs.push_back(std::pair<int,int>(patch1,patch2));
	}
	
	while(badPatchPairs.size()>0)
	{
		std::map<int,int> freqCount;

		int highestFreq = -1;
		int highestPatch = -1;
		for(auto &bpp : badPatchPairs)
		{
			if (freqCount.count(bpp.first) == 0)
				freqCount[bpp.first] = 0;
			if (freqCount.count(bpp.second) == 0)
				freqCount[bpp.second] = 0;
			freqCount[bpp.first]++;
			freqCount[bpp.second]++;
			
			if (highestFreq==-1 || freqCount[bpp.first]>highestFreq)
			{
				highestFreq = freqCount[bpp.first];
				highestPatch = bpp.first;
			}
			if (freqCount[bpp.second]>highestFreq)
			{
				highestFreq = freqCount[bpp.second];
				highestPatch = bpp.second;
			}
		}
		
		printf("Bad patch found:%d\n",highestPatch);
		badPatches.insert(highestPatch);
		for(std::list<std::pair<int,int>>::iterator i = badPatchPairs.begin(); i != badPatchPairs.end();)
		{
			if (i->first==highestPatch || i->second==highestPatch)
				i=badPatchPairs.erase(i);
			else
				i++;
		}
	}
	
}

// More general bad patch finder that works on sequence of patches of specified length, looking for distance mismatch between the first
// and last members of the sequence (assuming that bad patches from smaller length sequences are already found)
void BadPatchFinder::FindBadPatchesGeneral(AlignmentMap &am, std::map<int,Patch> *patches, int length, std::set<int> &badPatches, std::vector<std::tuple<int,int,float>> &badPatchScores)
{
	std::list<std::vector<int>> badPatchTuples;

	PrecomputeNormals(patches);

	std::vector<int> indexedPatches;
	
	// Index patches.
	//
	// Only patches that have at least one alignment: a patch aligned to nothing cannot be part of
	// any chain, and the index initialisation below reads am[patch][0] without checking, so an
	// isolated patch segfaults there. It never happens in a single grown run, because a patch is
	// only kept when it aligns, but it happens as soon as patches from separate runs are put in
	// one set.
	for(auto &p : *patches)
	{
		AlignmentMap::const_iterator it = am.find(p.first);
		if (it == am.end() || it->second.empty()) continue;
		printf("%d\n",p.first);
		indexedPatches.push_back(p.first);
	}

	if (indexedPatches.size() < (size_t)length)
	{
		printf("Not enough aligned patches for chains of length %d\n",length);
		return;
	}
	
	std::vector<int> indices; // first index is index of patch, next are all index into alignment map vector
	
	for(int i = 0; i<length; i++)
		indices.push_back(0);

	indices[0] = indexedPatches.size()-1;
	
	printf("Initializing indices\n");
	int currentPatch = indexedPatches[indices[0]];
	printf("%d\n",currentPatch);
	for(int i = 1; i<length; i++)
	{
		printf("l=%d\n",(int)am[currentPatch].size());
		int p = std::get<0>(am[currentPatch][0]);
		printf("p=%d\n",p);
		indices[i] = 0;
		currentPatch = p;
		printf("%d\n",currentPatch);
	}
	printf("Done indices\n");

	std::vector<std::vector<int>> patchSequences;
	
	bool done = false;
	while(!done)
	{
		std::vector<int> currentSequence;
		std::set<int> currentSequencePatches;
		bool hasBadPatch = false, hasRepeat = false;
				
		// output a patch sequence only if it contains no bad patches
		int currentPatch = indexedPatches[indices[0]];
		if (badPatches.count(currentPatch)!=0) hasBadPatch=true;
		currentSequence.push_back(currentPatch);
		currentSequencePatches.insert(currentPatch);
		for(int i = 1; i<length; i++)
		{
			if (indices[i]<(int)am[currentPatch].size())
			{
				currentPatch = std::get<0>(am[currentPatch][indices[i]]);
				if (currentSequencePatches.count(currentPatch)!=0)
				{
					hasRepeat=true;
					break;
				}
				if (badPatches.count(currentPatch)!=0)
				{
					hasBadPatch=true;
					break;
				}
				currentSequence.push_back(currentPatch);
				currentSequencePatches.insert(currentPatch);
			}
			else
			{
				printf("Error in patch sequence\n");
				printf("currentPatch=%d, am[%d].size()=%d, indices[%d]=%d\n",currentPatch,currentPatch,(int)am[currentPatch].size(),i,indices[i]);
			}
		}
		
		if (!hasBadPatch && !hasRepeat)
		{
			// To avoid duplicating sequences, only save those where the first patch is < the last patch
			if (currentSequence.front() < currentSequence.back())
				patchSequences.push_back(currentSequence);
		}
		
		// increment onto the next sequence of patches
		bool advanceToNextIndex = true;
		for(int indexToInc=length-1; indexToInc>=0 && advanceToNextIndex; indexToInc--)
		{
			advanceToNextIndex = false;
			if (indexToInc==0)
				indices[indexToInc]--;
			else
				indices[indexToInc]++;

			if (indices[0]==0)
			{
				// finished incrementing
				done = true;
			}
			else
			{
				int currentPatch = indexedPatches[indices[0]];
				for(int i = 1; i<length; i++)
				{
					if (indices[i]<(int)am[currentPatch].size())
					{
						currentPatch = std::get<0>(am[currentPatch][indices[i]]);
					}
					else
					{
						indices[indexToInc]=0;
						advanceToNextIndex = true;
						break;
					}
				}
			}
		}
	}
/*
	for(auto &i : patchSequences)
	{
		for(auto &p : i)
		{
			printf("%d ",p);
		}
		
		printf("\n");
	}
*/
    printf("Iterating over patch sequences\n");
	
	// Each sequence is independent of the others. The parallel loop computes only the three
	// numbers that are needed, into vectors indexed by sequence; everything else (the printed
	// lines, the two lists, the flagged chains) is derived from those numbers in a serial pass
	// afterwards, in the original order. The output is therefore unchanged.
	const size_t numSeq = patchSequences.size();
	std::vector<float> seqMaxDistance(numSeq, 0.0f);
	std::vector<long> seqOverlap(numSeq, 0), seqBlind(numSeq, 0);

	{
		int nt = omp_get_max_threads();
		std::vector<RenderState> states(nt);

		#pragma omp parallel for schedule(dynamic,16)
		for(size_t q = 0; q<numSeq; q++)
		{
			RenderState &st = states[omp_get_thread_num()];
			st.Clear();

			const std::vector<int> &i = patchSequences[q];
			int count = 0;
			int lastPatch = -1;
			affineTx aftx = affineTx(1,0,0,0,1,0);

			for(int p : i)
			{
				if (count==0)
				{
					PlacePatchInto(st,patches->at(p),p,aftx,true);
				}
				else
				{
					for(const auto &al : am.at(p))
						if (std::get<0>(al)==lastPatch)
						{
							affineTx nextAftx(std::get<7>(al),std::get<8>(al),std::get<9>(al),std::get<10>(al),std::get<11>(al),std::get<12>(al));
							aftx = AffineTxMultiply(aftx,nextAftx);
						}

					if (count == (int)i.size()-1)
						PlacePatchInto(st,patches->at(p),p,aftx,false);
				}

				count++;
				lastPatch = p;
			}

			seqMaxDistance[q] = st.maxDistance;
			seqOverlap[q]     = st.cellsOverlap;
			seqBlind[q]       = st.cellsBlind;
		}
	}

	for(size_t q = 0; q<numSeq; q++)
	{
		const std::vector<int> &i = patchSequences[q];

		for(auto &p : i)
			printf("%d ",p);
		printf("\n");

		int p = i.back();
		badPatchScores.push_back(std::tuple<int,int,float>(i[0],p,seqMaxDistance[q]));
		pairStats.push_back(std::make_tuple(i[0],p,length,seqMaxDistance[q],seqOverlap[q],seqBlind[q]));
		printf("%d,%d,%f\n",i[0],p,seqMaxDistance[q]);

		if (seqMaxDistance[q] > BP_MAX_XYZ_DISTANCE*(length-1))
			badPatchTuples.push_back(i);
	}
	
	
	// At length 2 the tuples are pairs, so the choice of what to discard is the same vertex
	// cover as in FindBadPatches and goes through the same function. Longer chains are a
	// hypergraph and the loop below is left as it was.

	while(badPatchTuples.size()>0)
	{
		std::map<int,int> freqCount;

		int highestFreq = -1;
		int highestPatch = -1;
		for(auto &bpt : badPatchTuples)
		{
			for(auto i : bpt)
			{
				if (freqCount.count(i) == 0)
					freqCount[i] = 0;
				freqCount[i]++;
			
				if (highestFreq==-1 || freqCount[i]>highestFreq)
				{
					highestFreq = freqCount[i];
					highestPatch = i;
				}
			}
		}
		
		printf("Bad patch found:%d\n",highestPatch);
		badPatches.insert(highestPatch);
		for(std::list<std::vector<int>>::iterator bpt = badPatchTuples.begin(); bpt != badPatchTuples.end();)
		{
			for(auto i : *bpt)
			{
				if (i==highestPatch)
				{
					bpt=badPatchTuples.erase(bpt);
					break;
				}
			}
			
			bpt++;
		}
	}
}

void BadPatchFinder::FindNeighbourProblems(std::map<int,std::set<int> > &neighbourList, std::map<int,Patch> *patches, std::set<int> badBridges, std::map<int,int> &newBadBridges, std::vector<int> &patchOrder, std::map<int,affineTx> &patchPositions)
{
	for(auto p : patchOrder)
	{
		if (badBridges.count(p)!=0)
			continue;
		
		float x1,y1,angle1;
		
		AffineTxToXYA(patchPositions[p],x1,y1,angle1);
		
		std::vector<std::tuple<float,float,float,int>> neighbourPositions;
		
		for(auto &n : neighbourList[p])
		{
			if (badBridges.count(n)!=0)
				continue;
			
		    float x2,y2,angle2;
			AffineTxToXYA(patchPositions[n],x2,y2,angle2);
			
			neighbourPositions.push_back({x2,y2,(*patches)[n].radius,n});
		}
		
		// Find the distance between each pair of neighbours - if any of them exceeds
        for(unsigned i = 0; i<neighbourPositions.size(); i++)
		{
			for(unsigned j = i+1; j<neighbourPositions.size(); j++)
			{
				float d = Distance(std::get<0>(neighbourPositions[i]),std::get<1>(neighbourPositions[i]),std::get<0>(neighbourPositions[j]),std::get<1>(neighbourPositions[j]));
				
				// Max possible distance that these neighbours of A can be separated by is the diameter of A + the radius of each neighbour
				float maxPossibleDist = 2.0*(*patches)[p].radius+std::get<2>(neighbourPositions[i])+std::get<2>(neighbourPositions[j]);
				
				if (d>maxPossibleDist)
				{
					printf("Bad spanning patches: %d,%d,%d : %f,%f\n",p,std::get<3>(neighbourPositions[i]),std::get<3>(neighbourPositions[j]),d,maxPossibleDist);
					
					if (newBadBridges.count(p)==0)
						newBadBridges[p]=0;
					
					newBadBridges[p]++;
				}
			}
		}			
	}		
}

