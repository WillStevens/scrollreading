#pragma once

#include <vector>
#include <string>

#include "parameters.h"
#include "common_types.h"

#include "zarr_1_b700.h"

void ZarrShow2U8(ZARR_1_b700 *za, int xcoord, int ycoord, int zcoord, int width, int height, const std::string &tifName, std::vector<Patch *> patches, int bgr, int bgg, int bgb);
