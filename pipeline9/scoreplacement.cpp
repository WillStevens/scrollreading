#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <list>

#include "parameters.h"
#include "common_types.h"

float ScorePlacement(AlignmentMap *am, std::map<int,Patch> *patches,  std::vector<int> &patchOrder, std::set<int> &patchesToColour, std::set<std::pair<int,int>> &manualGoodRel, std::set<int> &patchesInvolved, int maxDistanceThresh, float stepSize=1.0, bool writeColours = true, bool showDD = false)
{

			
	std::map<int,int> distanceDistribution;

	//printf("Loaded patch positions\n");
				
	// Which patches could be in each x,y chunk?
	std::map<std::pair<int,int>,std::set<int>> patchIndex;
	int patchIndexScale = 16;
				
    // also work out xmin,xmax,ymin,ymax;
	
	float xmin=0,ymin=0,xmax=0,ymax=0;
	bool first = true;
	{
		for (auto i : patchOrder)
		{			
			if (patches->count(i)==0)
			{
				printf("Patch %d not found in 'patches'\n",i);
				exit(-1);
			}
			Patch &p = (*patches)[i];
			for(PatchIterator pi = p.Begin(); p.Next(pi);)
			{
				float x,y;
				p.TransformPoint(pi.p->x,pi.p->y,x,y);
				
				if (x<xmin || first) xmin=x;
				if (y<ymin || first) ymin=y;
				if (x>xmax || first) xmax=x;
				if (y>ymax || first) ymax=y;
				
				first = false;
				
				int xi = x/patchIndexScale;
				int yi = y/patchIndexScale;
							
				for(int xo = xi-1; xo <= xi+1; xo++)
				for(int yo = yi-1; yo <= yi+1; yo++)
				{
					if (patchIndex.count(std::pair<int,int>(xo,yo))==0)
					{
						patchIndex[std::pair<int,int>(xo,yo)] = std::set<int>();
					}
								
					patchIndex[std::pair<int,int>(xo,yo)].insert(i);
				}
								
			}
		}
	}
				
	//printf("Finished indexing patches\n");
	//printf("xmin,ymin,xmax,ymax=%f,%f,%f,%f\n",xmin,ymin,xmax,ymax);		
	float score = 0;
	float totalPoints = 0;
	
	// Now iterate over some coords...
	{		
		Patch outputPatch;
		std::vector<patchPoint> points;
		std::vector<std::tuple<int,int,int>> colours;
					
		std::set<std::list<int> > mismatches;

		/*			for(float x=-1600+719*2; x<=-1600+719*2+800; x+=1)
					{
						for(float y=-2600+1500*2; y<=-2600+1500*2+800; y+=1)
						{
		*/			
		for(float x=xmin; x<=xmax; x+=stepSize)
		{
			for(float y=ymin; y<=ymax; y+=stepSize)
			{
				std::vector<std::tuple<int,Vec3,Vec3>> contributions;
							
				int xi = x/patchIndexScale;
				int yi = y/patchIndexScale;
							
				if (patchIndex.count(std::pair<int,int>(xi,yi))!=0)
				{
					Vec3 totalV;
					float totalWeight = 0.0;
											
					//printf("{\n");
					for(auto &i : patchIndex[std::pair<int,int>(xi,yi)])
					{
						Vec3 v;
						Vec3 normal;
						float weight=0.0;
									
						if ((*patches)[i].FindGlobalXY(x,y,v,normal,weight))						
							if (weight>0.0)
							{
								//printf("patch=%d v=%f,%f,%f weight=%f\n",i,v.x,v.y,v.z,weight);
								totalV += v*weight;
								totalWeight += weight;
										
								contributions.push_back(std::tuple<int,Vec3,Vec3>(i,v,normal));
							}
					}
					//printf("}\n");

					// Look for the largest distance between pairs of contributions, in the direction of normals
					
					// If there are any contributions, increase the score by 1 for area covered
					if (contributions.size()>0)
					{
						score+=1.0;
						totalPoints+=1.0;
					}
					
					float maxDistance = 0;
					for(int i=0; i<(int)contributions.size(); i++)
					{
						for(int j=i+1; j<(int)contributions.size(); j++)
						{
							Vec3 posi = std::get<1>(contributions[i]);
							Vec3 normali = std::get<2>(contributions[i]);
							Vec3 posj = std::get<1>(contributions[j]);
							Vec3 normalj = std::get<2>(contributions[j]);
										
							float distancei = fabs(Vec3::dot(normali,posi - posj));
							float distancej = fabs(Vec3::dot(normalj,posi - posj));
										
							float distance = distancei>distancej ? distancei : distancej;

							// Don't report on any that are to be excluded from report
							if (distance > maxDistance &&
							  manualGoodRel.count({std::get<0>(contributions[i]),std::get<0>(contributions[j])})==0 &&
							  manualGoodRel.count({std::get<0>(contributions[j]),std::get<0>(contributions[i])})==0)
							{
								maxDistance = distance;
								patchesInvolved.insert(std::get<0>(contributions[i]));
								patchesInvolved.insert(std::get<0>(contributions[j]));
							}
										
							int distance_i = (int)distance;
										
							if (distanceDistribution.count(distance_i)==0)
							{
								distanceDistribution[distance_i] = 0;
							}
										
							distanceDistribution[distance_i]++;
						}
					}

					// decrease score - bigger decrease is points are a long distance apart
					score -= maxDistance/30;
					
					if (maxDistanceThresh!=-1 && maxDistance>(float)maxDistanceThresh)
					{
						//printf("Distance violation (%f) when flattening at %f,%f\n",maxDistance,x,y);
						//for(auto &c : contributions)
						//{
						//	printf("%d : %f,%f,%f\n",std::get<0>(c),std::get<1>(c).x,std::get<1>(c).y,std::get<1>(c).z);
						//}
									
						std::list<int> mismatch;
						for(auto &c : contributions)
						{
							mismatch.push_back(std::get<0>(c));
						}
									
									
						mismatches.insert(mismatch);
					}

					if (totalWeight != 0.0)
					{
						totalV /= totalWeight;
						points.push_back(patchPoint(x,y,totalV.x,totalV.y,totalV.z));
									
						int r=255,g=255,b=255;
									
						std::vector<int> colourableContribs;
									
						for(auto c : contributions)
						{
							int patch = std::get<0>(c);
							if (patchesToColour.count(patch)!=0)
								colourableContribs.push_back(patch);
						}
								
						if (colourableContribs.size()>0)
						{
							int n = rand()%colourableContribs.size();
							PatchNumberToColour(colourableContribs[n],r,g,b);
						}

						// Use a red hue to indicate thickness
						r += (int)maxDistance*4;
						g -= (int)maxDistance*4;
						b -= (int)maxDistance*4;
									
						if (r>255) r=255;
						if (g<0) g=0;
						if (b<0) b=0;
									
						colours.push_back({r,g,b});
					}

					//printf("%f,%f has coords %f,%f,%f\n",x,y,totalV.x,totalV.y,totalV.z); 
							
				}
							
			}
		}
		
		if (writeColours)
			outputPatch.BuildFromPoints(points,colours,0);
		else
			outputPatch.BuildFromPoints(points,0);
			
		outputPatch.Write(OUTPUT_DIR,0);
		
		if (showDD)
		{
			printf("Distance distribution\n");
			for(const auto &dd : distanceDistribution)
			{
				printf("%d,%d\n",dd.first,dd.second);
			}
		}
		
/*		printf("Mismatches\n");
		for(const auto &mm : mismatches)
		{
			for(const auto &m : mm)
			{
				printf("%d ",m);
				patchesInvolved.insert(m);
			}
			printf("\n");
		}
*/					
/*
		for(auto p : patchesInvolved)
		{
			printf("%d ",p);
		}
		printf("\n");
*/
	}
	
//	printf("Score and total points=%f,%f\n",score,totalPoints);
	return score;
}

