#pragma once

// Given two patches, work out the best rotatation and offset for the second one to align it with the first

#include <tuple>
#include <vector>
#include <map>

#include "parameters.h"

#include "bigpatch.h"
#include "common_types.h"

typedef std::tuple<float,float,float,float,int> match;

typedef std::tuple<int,int,int> gridCell;

typedef std::map< gridCell, std::vector<gridPoint> >::iterator cellMapIterator;

class Aligner
{
	public:
	    Aligner() {}
		~Aligner() {}
		
		std::vector<gridPoint> gridPoints[2];

		std::map< gridCell, std::vector<gridPoint> > cellMap0;
		std::map< gridCell, std::vector<gridPoint> > cellMap1;


        bool AlignPatches(BigPatch *bp, Patch &p, std::vector<alignment> &alignments);

		void FillCellMap(void);
		void FindMatches(std::map<int,std::vector<match>> &matchList);
		// This needs to return a list of patch number, variance, transform
		bool AlignMatches(std::map<int, std::vector<match>> &matchListMap, std::vector<alignment> &alignments);
};