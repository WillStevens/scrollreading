#pragma once

#include <map>
#include <vector>

#include "common_types.h"

// Add the reversals too (keep the variances the same as the original)
void AugmentAlignmentMap(AlignmentMap &alignmentMap);
// Given patches and alignments, work out global positions.
void PositionPatches(std::map<int,Patch> *patches, AlignmentMap &alignmentMap, std::vector<int> &patchOrder, std::map<int,affineTx> &patchPositions, std::vector<std::pair<int,alignment>> &alignmentOrder);

