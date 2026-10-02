#pragma once

// Given two patches, work out the best rotatation and offset for the second one to align it with the first

#include <tuple>
#include <vector>
#include <map>

#include "parameters.h"

#include "bigpatch.h"
#include "common_types.h"
#include "cellgrid.h"

typedef std::tuple<float,float,float,float,int> match;

typedef std::tuple<int,int,int> gridCell;


class Aligner
{
	public:
	    Aligner() {}
		~Aligner() {}
		
		std::vector<gridPoint> gridPoints[2];

		// the cells of gridPoints[0] and gridPoints[1]; a flat hash in place of std::map< gridCell, std::vector<gridPoint> >
		CellGrid<gridPoint> cellMap0;
		CellGrid<gridPoint> cellMap1;


        bool AlignPatches(BigPatch *bp, Patch &p, std::vector<alignment> &alignments);
		bool AlignPatches(Patch &p0, Patch &p1, std::vector<alignment> &alignments);

		void FillCellMap(void);
		void FindMatches(std::map<int,std::vector<match>> &matchList);
		// This needs to return a list of patch number, variance, transform
		bool AlignMatches(std::map<int, std::vector<match>> &matchListMap, std::vector<alignment> &alignments);
};