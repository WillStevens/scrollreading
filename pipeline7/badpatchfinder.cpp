#include "badpatchfinder.h"

void BadPatchFinder::ClearRendered(void)
{
	memset(rendered_i,0,R_ARRAY_SIZE*R_ARRAY_SIZE*sizeof(int));
	memset(rendered_f,0,R_ARRAY_SIZE*R_ARRAY_SIZE*sizeof(float)*3);
	memset(distances,0,D_SIZE_X*D_SIZE_Y*sizeof(float));
	
	maxDistance = 0.0;
}

void BadPatchFinder::PlacePatch(const Patch &p1, int patchNum,const affineTx &aftx)
{
	for(auto const &point : p1.points)
	{
		float x,y,px,py,pz;
		x=point.x; y=point.y; px=point.v.x; py=point.v.y; pz=point.v.z;
								
		//x=x*QUADMESH_SIZE;
		//y=y*QUADMESH_SIZE;
								
		// transform to get position of this patch point
		AffineTxApply(aftx,x,y);
		
		int xrdi = ROUND(x/RESCALE);
		int yrdi = ROUND(y/RESCALE);

		if (rendered_i[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2]==0)
		{
			rendered_i[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2] = patchNum+1;
			rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][0] = px;
			rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][1] = py;
			rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][2] = pz;

#ifdef COLLECT_DISTANCE_DISTRIB
            distances[xrdi+D_OFF_X][yrdi+D_OFF_Y] = 1; // don't use zero, because that means 'empty'
#endif
		}
        else
		{
			int ePatchNum = rendered_i[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2]-1;
					
			float ex = rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][0];
			float ey = rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][1];
			float ez = rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][2];

			if (ePatchNum != patchNum)
			{
				//printf("%d,%d : %f,%f,%f vs %f,%f,%f\n",ePatchNum,patchNum,ex,ey,ez,px,py,pz);
				float distance = Distance(ex,ey,ez,px,py,pz); 
					
#ifdef COLLECT_DISTANCE_DISTRIB
				distances[xrdi+D_OFF_X][yrdi+D_OFF_Y] = distance+1;
#endif					
				
				if (distance > maxDistance) maxDistance = distance;					
			}
		}
	}
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

// Given an alignment map and the patches, iterate through all neighbouring patches
// and find patches with mismatch between 2D x,y and 3D x,y,z
void BadPatchFinder::FindBadPatches(const AlignmentMap &am, std::map<int,Patch> *patches, std::set<int> &badPatches, std::vector<std::tuple<int,int,float>> &badPatchScores)
{
	std::list<std::pair<int,int>> badPatchPairs;
		
	// Iterate over patches
	for(auto a : am)
	{
		int patch1 = a.first;
		
		// Iterate over alignments
		for(auto al : a.second)
		{
			ClearRendered();
	
			int patch2 = std::get<0>(al);
			
			affineTx aftx(std::get<7>(al),std::get<8>(al),std::get<9>(al),std::get<10>(al),std::get<11>(al),std::get<12>(al));
			
			PlacePatch((*patches)[patch2],patch2,affineTx(1,0,0,0,1,0));
			PlacePatch((*patches)[patch1],patch1,aftx);
			
			printf("%d,%d : %f\n",patch2,patch1,maxDistance);
			badPatchScores.push_back(std::tuple<int,int,float>(patch2,patch1,maxDistance));
			
			if (maxDistance > BP_MAX_XYZ_DISTANCE)
			{
				badPatchPairs.push_back(std::pair<int,int>(patch1,patch2));
			}
			
#ifdef OUTPUT_DISTANCE_TIF
			RenderDistances();
#endif
		}
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
	
	std::vector<int> indexedPatches;
	
	// Index patches, not including known bad patches
	for(auto &p : *patches)
		indexedPatches.push_back(p.first);
	
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
		bool hasBadPatch = false;
				
		// output a patch sequence only if it contains no bad patches
		int currentPatch = indexedPatches[indices[0]];
		if (badPatches.count(currentPatch)!=0) hasBadPatch=true;
		currentSequence.push_back(currentPatch);
		for(int i = 1; i<length; i++)
		{
			if (indices[i]<am[currentPatch].size())
			{
				currentPatch = std::get<0>(am[currentPatch][indices[i]]);
				if (badPatches.count(currentPatch)!=0) hasBadPatch=true;
				currentSequence.push_back(currentPatch);
			}
			else
			{
				printf("Error in patch sequence\n");
			}
		}
		
		if (!hasBadPatch)
			patchSequences.push_back(currentSequence);
		printf("\n");
		
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
					if (indices[i]<am[currentPatch].size())
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
	for(auto &i : patchSequences)
	{
		ClearRendered();
		int count = 0;
		int lastPatch = -1;
		affineTx aftx = affineTx(1,0,0,0,1,0);
		for(auto &p : i)
		{
			if (count==0)
			{
				PlacePatch((*patches)[p],p,aftx);
			}
			else
			{
				for(auto &al : am[lastPatch])
					if (std::get<0>(al)==p)
					{
						affineTx nextAftx(std::get<7>(al),std::get<8>(al),std::get<9>(al),std::get<10>(al),std::get<11>(al),std::get<12>(al));
						
						aftx = AffineTxMultiply(aftx,AffineTxInverse(nextAftx));
					}
					
				if (count == i.size()-1)
				{
					PlacePatch((*patches)[p],p,aftx);
					
					badPatchScores.push_back(std::tuple<int,int,float>(i[0],p,maxDistance));
					printf("%d,%d,%f\n",i[0],p,maxDistance);
					
					if (maxDistance > BP_MAX_XYZ_DISTANCE*(length-1))
					{
						badPatchTuples.push_back(i);
					}
				}
			}
			
			count++;
			lastPatch = p;
		}
	}
	
	
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