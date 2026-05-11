#include <sstream>
#include <fstream>
#include <iomanip>

#include "sliceanimrender.h"
#include "zarr_show2_u8.h"

void SliceAnimRender(ZARR_1_b700 *za,const std::string &path, int patchesper, int zstep, int zscale, int closeUpIter, std::map<int,Patch> *patches, std::vector<int> &patchOrder)
{
	std::vector<Patch *> pNorm;

	bool forward = true, doneCloseUp = false;
	int iter = 0, closeUpStart = 0;
	for(int i = 0; i<patchOrder.size(); i+=patchesper)
	{
		printf("At patch %d\n",patchOrder[i]);
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
	
			pNorm.push_back(p);
		}
		
		// Round zmin and zmax to bounding zstep multiples
		zmin = (zmin/zstep)*zstep;
		zmax = ((zmax+zstep-1)/zstep)*zstep;
		
		// Total number of iteration that we'll do is (zmax-zmin)/step + 1
		// e.g. 0,10,5 -> 3 steps (0,5,10)
		//      0,10,3 -> 4 steps (0,3,6,9)
		int numIters = ((zmax-zmin)/zstep)+1;
	
		if (closeUpIter >= iter && closeUpIter < iter+numIters)
		{
			zstep = 10;
			doneCloseUp = true;
			closeUpStart = iter;
		}
		
		for(int z = (forward?zmin:zmax); forward?(z<=zmax):(z>=zmin); z+=(forward?zstep:-zstep))
		{
			std::stringstream oss;
			oss << path << (doneCloseUp?"/c_":"/s_") << std::setfill('0') << std::setw(8) << (doneCloseUp?iter-closeUpStart:iter);
			std::string fileNameRoot = oss.str();
			printf("Writing slice %d to %s\n",iter,fileNameRoot.c_str());
			if (closeUpIter == -1 || doneCloseUp)
			{
				std::set<Patch *> pShown;

				ZarrShow2U8(za,VOL_OFFSET_X,VOL_OFFSET_Y,z,VOL_SIZE_X,VOL_SIZE_Y,fileNameRoot+".tif",pNorm,pShown,forward?32:0,0,forward?0:32);
				// Output details about which patche are in this slice
				{
					std::ofstream os(fileNameRoot+".csv");
					os << z << ":";
					bool first=true;
					for(auto &p : pShown)
					{
						os << (first?"":",") << p->GetPatchNum();
						first=false;
					}
					os << std::endl;
				}
			}
			iter++;
		}
		
		if (doneCloseUp)
			return;
		
		forward = !forward;
	}
}
