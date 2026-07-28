#pragma once

#include <map>
#include <vector>
#include <set>

float ScorePlacement(AlignmentMap *am, std::map<int,Patch> *patches,  std::vector<int> &patchOrder, std::set<int> &patchesToColour, std::set<std::pair<int,int>> &manualGoodRel, std::set<int> &patchesInvolved, int maxDistanceThresh, float stepSize=1.0, bool writeColours = true, bool showDD=false);
