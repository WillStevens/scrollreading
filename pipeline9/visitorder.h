#pragma once

#include <map>
#include <unordered_map>
#include <set>
#include <vector>

#include "common_types.h"

void MakeVisitOrder(AlignmentMap *am, std::map<int,Patch> *patches,std::set<int> &badPatches,std::set<std::pair<int,int>> &manualBadRel, std::vector<int> &patchOrder, std::vector<std::pair<int,alignment>> &alignmentOrder,std::map<int,affineTx> &patchPositions, std::map<int,std::set<int> > &neighbourList, AlignmentScoreMap &patchAlignmentScores,bool showSize=false, bool saveOutput=true);

int MakeVisitOrders(int N, AlignmentMap *am, std::map<int,Patch> *patches,std::set<int> &badPatches,std::set<std::pair<int,int>> &manualBadRel, std::vector< std::vector<int>> &patchOrders, std::vector<std::vector<std::pair<int,alignment>>> &alignmentOrders,std::vector<std::map<int,affineTx>> &patchPositionss, std::map<int,std::set<int> > &neighbourList, AlignmentScoreMap &patchAlignmentScores, bool showSize=false, bool saveOutput = true);
