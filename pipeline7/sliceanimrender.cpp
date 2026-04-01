#include <sstream>
#include <iomanip>

#include "sliceanimrender.h"
#include "zarr_show2_u8.h"

void SliceAnimRender(ZARR_1 *za,const std::string &path, int patchesper, int zstep, int zscale, std::map<int,Patch> *patches, std::vector<int> &patchOrder)
{
	std::vector<Patch *> pNorm,pHighlight;

	bool forward = true;
	int iter = 0;
	for(int i = 0; i<patchOrder.size(); i+=patchesper)
	{
		int zmin,zmax;
		
		// Find min and max z for these slices
		for(int j=i; j<i+patchesper && j<patchOrder.size(); j++)
		{
			int patchNum = patchOrder[j];
			Patch *p = &(*patches)[patchNum];
			
			if (p->MinZ() < zmin || j==i)
				zmin = p->MinZ();
			if (p->MaxZ() > zmax || j==i)
				zmax = p->MaxZ();
	
			pHighlight.push_back(p);
		}
		
		for(int z = (forward?zmin:zmax); forward?(z<=zmax):(z>=zmin); z+=(forward?zstep:-zstep))
		{
			std::stringstream oss;
			oss << path << "/s_" << std::setfill('0') << std::setw(8) << iter << ".tif";
			std::string tifName = oss.str();
			ZarrShow2U8(za,VOL_OFFSET_X,VOL_OFFSET_Y,z,VOL_SIZE_X,VOL_SIZE_Y,tifName,pNorm,pHighlight);
			iter++;
		}
		
		pNorm.insert(pNorm.end(),pHighlight.begin(),pHighlight.end());
		
		forward = !forward;
	}
}
