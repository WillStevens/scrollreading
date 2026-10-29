#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <set>

#include "common_types.h"

void OmissionTest(AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums,
            std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel,
            std::set<int> badBridges, AlignmentScoreMap &patchAlignmentScores, int N);
