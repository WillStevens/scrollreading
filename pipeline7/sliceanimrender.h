#include <map>
#include <vector>
#include <string>

#include "parameters.h"
#include "common_types.h"

void SliceAnimRender(const std::string &path, int slicesper, int zstep, int zscale, std::map<int,Patch> *patches, std::vector<int> patchOrder)
{
	std::vector<int> patchesToRender;
	
	for(int i = 0; i<patchOrder.size(); i+=slicesper)
	{
		int zmin,zmax;
		
		// Find min and max z for these slices
		for(int j=i; j<i+slicesper && j<patchOrder.size(); j++)
		{
			int patchNum = patchOrder[j];
			Patch &p = (*patches)[patchNum];
			
			if (p.MinZ() < zmin || j==i)
				zmin = p.MinZ();
			if (p.MaxZ() > zmax || j==i)
				zmax = p.MaxZ();
	
			patchesToRender.push_back(patchNum);
		}
		
		for(int z = zmin; z<=zmax; z+=zstep)
		{
			RenderZSlice(??? z,patchesToRender);
		}
	}
}
