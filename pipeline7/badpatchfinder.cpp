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
void BadPatchFinder::FindBadPatches(const AlignmentMap &am, std::map<int,Patch> *patches, std::set<int> &badPatches)
{
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
			
#ifdef OUTPUT_DISTANCE_TIF
			RenderDistances();
#endif
		}
	}
}