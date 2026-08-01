#pragma once

void Anneal(AlignmentMap *am, std::map<int,Patch> *patches, std::vector<int> patchNums, std::set<int> badPatches, std::set<std::pair<int,int>> manualBadRel, std::set<int> badBridges, int iterations, int N, float initT0=-1.0);
