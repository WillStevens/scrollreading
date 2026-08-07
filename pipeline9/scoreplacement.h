#pragma once

#include <map>
#include <vector>
#include <set>

float ScorePlacementAreaAndIncon(AlignmentMap *am, std::map<int,Patch> *patches, std::unordered_map<int,std::tuple<float,float,float>> &patchPositionsXYA, std::vector<int> &patchOrder, std::set<int> &patchesToColour, std::set<std::pair<int,int>> &manualGoodRel, std::set<int> &patchesInvolved, int maxDistanceThresh, float stepSize=1.0, bool writePatch = true, bool writeColours = true, bool showDD = false);

float ScorePlacementAreaOnly(AlignmentMap *am, std::map<int,Patch> *patches, std::unordered_map<int,std::tuple<float,float,float>> &patchPositionsXYA, std::vector<int> &patchOrder, std::set<int> &patchesToColour, std::set<std::pair<int,int>> &manualGoodRel, std::set<int> &patchesInvolved, int maxDistanceThresh, float stepSize=1.0, bool writePatch = true, bool writeColours = true, bool showDD=false);

float ScorePlacement(AlignmentMap *am, std::map<int,Patch> *patches, std::unordered_map<int,std::tuple<float,float,float>> &patchPositionsXYA, std::vector<int> &patchOrder, std::set<int> &patchesToColour, std::set<std::pair<int,int>> &manualGoodRel, std::set<int> &patchesInvolved, int maxDistanceThresh, float stepSize=1.0, bool writePatch = true, bool writeColours = true, bool showDD=false);
