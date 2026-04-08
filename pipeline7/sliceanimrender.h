#pragma once

#include <map>
#include <vector>
#include <string>

#include "parameters.h"
#include "common_types.h"
#include "zarr_1_b700.h"

void SliceAnimRender(ZARR_1_b700 *za,const std::string &path, int patchesper, int zstep, int zscale, std::map<int,Patch> *patches, std::vector<int> &patchOrder);
