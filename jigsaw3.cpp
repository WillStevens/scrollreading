/*
# Will Stevens, September 2024
# 
# Assemble surfaces from patches piece-by-piece.
#
# Surface point lists are in folders call sNNN, named vNNN_X.csv, where X is an arbitrary number that got
# assigned to a surface during processing.
#
# Released under GNU Public License V3
*/

#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <math.h>
#include <stdlib.h>

#include <unordered_set>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <utility>

#define SIZE 512
#define PT_MULT 1024

#define PROJECTION_SIZE 2048
#define PROJECTION_BLANK INT32_MIN
int32_t projection[PROJECTION_SIZE][PROJECTION_SIZE];

std::vector<std::string> files;
std::map<std::string,std::vector<int16_t> > extents;
std::map<std::string,std::unordered_set<uint32_t> > pointsets;
std::map<std::string,std::unordered_set<uint32_t> > pointsets_xz;
std::map<std::string,std::unordered_set<uint32_t> > plugs;
std::map<std::string,std::unordered_set<uint32_t> > plugs_xz;

std::map<std::string,int> groupPenaltyCache;

// Two vectors that define the projection plane
// The third vector in this array is the normal to the projection plane, calculated from the other two
int32_t planeVectors[3][3] = 
	{ { 1, 0, 0 },
	  { 0, 0, 1 } };

int32_t projectN(int x, int y, int z, int n)
{
	int32_t r = (x*planeVectors[n][0]+y*planeVectors[n][1]+z*planeVectors[n][2])/1000;
	
	return r;
}
	  
std::vector<int16_t> LoadExtent(const char *file, bool dilate)
{
	int xmin,ymin,zmin,xmax,ymax,zmax;
	
    std::vector<int16_t> extent;
	
	FILE *f = fopen(file,"r");
	
	while(f)
	{
		if (fscanf(f,"%d,%d,%d,%d,%d,%d",&xmin,&ymin,&zmin,&xmax,&ymax,&zmax)==6)
		{
			extent.push_back(xmin-(dilate?1:0));
			extent.push_back(ymin-(dilate?1:0));
			extent.push_back(zmin-(dilate?1:0));
			extent.push_back(xmax+(dilate?1:0));
			extent.push_back(ymax+(dilate?1:0));
			extent.push_back(zmax+(dilate?1:0));
		}
		else
			break;
	}
	
	fclose(f);
	
	return std::move(extent);
}

std::unordered_set<uint32_t> LoadVolume(const char *file, bool dilate)
{
	int x,y,z;
	
    std::unordered_set<uint32_t> volume;
	
	FILE *f = fopen(file,"r");
	
	while(f)
	{
		if (fscanf(f,"%d,%d,%d",&x,&y,&z)==3)
		{
			for(int xo=(dilate?-1:0); xo<=(dilate?1:0); xo++)
			for(int yo=(dilate?-1:0); yo<=(dilate?1:0); yo++)
			for(int zo=(dilate?-1:0); zo<=(dilate?1:0); zo++)
			  volume.insert( ((y+1+yo)*PT_MULT+x+1+xo)*PT_MULT+z+1+zo );
		}
		else
			break;
	}
	
	fclose(f);
	
	return std::move(volume);
}

std::unordered_set<uint32_t> UnorderedSetIntersection(std::unordered_set<uint32_t> &a,std::unordered_set<uint32_t> &b)
{
	if (a.size()<=b.size())
	{
		std::unordered_set<uint32_t> r;
	
		for(uint32_t x : a)
		{
			if (b.find(x)!=b.end())
				r.insert(x);
		}
	
		return std::move(r);
	}
	else
		return UnorderedSetIntersection(b,a);
}

std::unordered_set<uint32_t> UnorderedSetDifference(std::unordered_set<uint32_t> &a,std::unordered_set<uint32_t> &b)
{
	std::unordered_set<uint32_t> r;
	
	for(uint32_t x : a)
	{
		if (b.find(x)==b.end())
			r.insert(x);
	}
	
	return std::move(r);
}

int UnorderedSetIntersectionSize(std::unordered_set<uint32_t> &a,std::unordered_set<uint32_t> &b)
{	
	if (a.size()<=b.size())
	{
		int r = 0;
	
		for(uint32_t x : a)
		{
			if (b.find(x)!=b.end())
				r++;
		}
	
	    //printf("USIS:%d\n",r);
		return r;
	}
	else
		return UnorderedSetIntersectionSize(b,a);
}

bool PointInCube(int x, int y, int z, int cxmin, int cymin, int czmin, int cxmax, int cymax, int czmax)
{
	return x>=cxmin && x<=cxmax && y>=cymin && y<=cymax && z>=czmin && z<=czmax;
}

int Abutting(std::unordered_set<uint32_t> &ps1,
			 std::unordered_set<uint32_t> &plugs1,
			 std::unordered_set<uint32_t> &ps1_xz,
			 std::unordered_set<uint32_t> &plugs1_xz,
             std::vector<int16_t> &e1,
		     std::unordered_set<uint32_t> &ps2,
		     std::unordered_set<uint32_t> &plugs2,
		     std::unordered_set<uint32_t> &ps2_xz,
		     std::unordered_set<uint32_t> &plugs2_xz,
			 std::vector<int16_t> &e2
			)
{

  int xmin1=e1[0], ymin1=e1[1], zmin1=e1[2], xmax1=e1[3], ymax1=e1[4], zmax1=e1[5];
  int xmin2=e2[0], ymin2=e2[1], zmin2=e2[2], xmax2=e2[3], ymax2=e2[4], zmax2=e2[5];
  
  bool overlap = false;

  if (PointInCube(xmin1,ymin1,zmin1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmax1,ymin1,zmin1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmin1,ymax1,zmin1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmax1,ymax1,zmin1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmin1,ymin1,zmax1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmax1,ymin1,zmax1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmin1,ymax1,zmax1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmax1,ymax1,zmax1,xmin2,ymin2,zmin2,xmax2,ymax2,zmax2))
	  overlap=true;
  else if (PointInCube(xmin2,ymin2,zmin2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmax2,ymin2,zmin2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmin2,ymax2,zmin2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmax2,ymax2,zmin2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmin2,ymin2,zmax2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmax2,ymin2,zmax2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmin2,ymax2,zmax2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;
  else if (PointInCube(xmax2,ymax2,zmax2,xmin1,ymin1,zmin1,xmax1,ymax1,zmax1))
	  overlap=true;

  if (!overlap)
    return 0;
   
  
  int common = UnorderedSetIntersectionSize(plugs1,plugs2);

  //printf("p1:%d p2:%d c:%d\n",(int)plugs1.size(),(int)plugs2.size(),common);

  if (common==0)
	  return 0;
  
  // When working out the intersection , discount points projecting to the same points as plugs
  std::unordered_set<uint32_t> intersection = UnorderedSetIntersection(ps1_xz,ps2_xz);
  intersection = UnorderedSetDifference(intersection,plugs1_xz);
  intersection = UnorderedSetDifference(intersection,plugs2_xz);
  
  int intersectionSize = intersection.size();
  
  //printf("i:%d, c:%d\n",intersectionSize,common);
  
  if (intersectionSize > common*20)
	  return 0;
  
  return common;
}


std::vector<int16_t> UpdateExtent(std::vector<int16_t> &a, std::vector<int16_t> &b)
{
  if (a.size()==0)
	return b;
  if (b.size()==0)
	return a;

  std::vector<int16_t> r;
  
  r.push_back((a[0]<b[0])?a[0]:b[0]);
  r.push_back((a[1]<b[1])?a[1]:b[1]);
  r.push_back((a[2]<b[2])?a[2]:b[2]);
  r.push_back((a[3]>b[3])?a[3]:b[3]);
  r.push_back((a[4]>b[4])?a[4]:b[4]);
  r.push_back((a[5]>b[5])?a[5]:b[5]);
  
  return r;
}

int EvaluateSolution(std::vector<std::tuple<std::string,std::string,int> > &possibleNeighbours, std::vector<bool> &solution)
{
	int score = 0, penalty = 0;
	
	int nextFreeGroupId = 0;
	
	static int projIndex = INT32_MIN+1;
	
	std::set<int> groupIds;
	std::map<std::string,int> groupMap;
	
	// Work out which patches are in which sets
	for(int i = 0; i<solution.size(); i++)
	{		
		if (solution[i])
		{
			std::string f1 = std::get<0>(possibleNeighbours[i]);
			std::string f2 = std::get<1>(possibleNeighbours[i]);
			int abutScore = std::get<2>(possibleNeighbours[i]);

			// Add to the score based on the size of the overlap
			score += abutScore;
			
			if (groupMap.count(f1) == 0 && groupMap.count(f2) == 0)
			{
				groupIds.insert(nextFreeGroupId);
				groupMap[f1] = groupMap[f2] = nextFreeGroupId++;
			}
			else if (groupMap.count(f1) == 0)
			{
				groupMap[f1] = groupMap[f2];
			}
			else if (groupMap.count(f2) == 0)
			{
				groupMap[f2] = groupMap[f1];
			}
			else if (groupMap[f2] != groupMap[f1])
			{
				int toRelabel = groupMap[f2];
				int newLabel = groupMap[f1];
				for(std::map<std::string,int>::iterator iter = groupMap.begin(); iter != groupMap.end(); ++iter)
				{
					std::string i = iter->first;
					if (groupMap[i]==toRelabel)
						groupMap[i] = newLabel;
				}		

				groupIds.erase(toRelabel);
			}
		}
	}
	
	int maxGroupSize = 0;
	int maxGroupNumPoints = 0;
	std::string groupWithMax;
	
	for(int i : groupIds)
	{
//		printf("GroupId %d\n",i);

        int groupPenalty = 0;
		int groupNumPoints = 0;
		
    	int minProjIndex = projIndex;
		
        std::vector<std::string> filesInGroup;

        std::string groupString;		
		for(std::map<std::string,int>::iterator iter = groupMap.begin(); iter != groupMap.end(); ++iter)
		{
			if (iter->second==i)
			{
			    filesInGroup.push_back(iter->first);
				groupString += iter->first + std::string(":");
				
				groupNumPoints += pointsets[iter->first].size();
			}
		}

		if (groupNumPoints > maxGroupNumPoints)
		{
			maxGroupNumPoints = groupNumPoints;
			groupWithMax = groupString;
		}
		
		if (groupPenaltyCache.count(groupString)==1)
		{
			penalty += groupPenaltyCache[groupString];
		}
		else
		{
			for(const std::string& file : filesInGroup)
			{
				//printf("Here A\n");
				std::unordered_set<uint32_t> s = UnorderedSetDifference(pointsets_xz[file],plugs_xz[file]); 
				for(uint32_t u : s)
				{
					int pz = (u%2048)-1;
					int px = (u/2048)-1;

					//printf("u:%d px:%d pz:%d\n",u,px,pz);
					
					if (projection[px][pz]<minProjIndex)
					{
						projection[px][pz]=projIndex;
					}
					else if (projection[px][pz]!=projIndex)
					{
				        groupPenalty++;
					    projection[px][pz]=projIndex;
					}
				}
				//printf("Here B\n");
				
				projIndex++;	
			}

			groupPenaltyCache[groupString] = groupPenalty;
			
			penalty += groupPenalty;
		}
		
		if (filesInGroup.size() > maxGroupSize)
			maxGroupSize = filesInGroup.size();
		
	}
	
	printf("Groups:%d, maxGroupSize:%d, maxGroupNumPoints:%d, Score: %d, Penalty: %d\n",(int)groupIds.size(),maxGroupSize,maxGroupNumPoints,score,penalty);
	printf(groupWithMax.c_str());
	
	return 25*score - 5*penalty;
}

class RunStats
{
	public:
		std::vector<int> patchSizes;
		std::vector<int> surfaceSizes;
		
		void Display(void)
		{
			std::sort(patchSizes.begin(),patchSizes.end());
			std::sort(surfaceSizes.begin(),surfaceSizes.end());
			
			int totalPatchSize = 0;
			for(int s : patchSizes)
			{
				totalPatchSize += s;
			}

			int totalSurfaceSize = 0;
			int numLargeSurfaces = 0;
			for(int s : surfaceSizes)
			{
				totalSurfaceSize += s;
				if (s>SIZE*SIZE/2)
				  numLargeSurfaces++;	
			}
			
			int medianPatchSize=0;
			if (patchSizes.size()%2==0 && patchSizes.size()>0)
			{
				medianPatchSize = (patchSizes[patchSizes.size()/2-1]+patchSizes[patchSizes.size()/2])/2;
			}
			else if (patchSizes.size()%2==1 && patchSizes.size()>0)
			{
				medianPatchSize = patchSizes[patchSizes.size()/2];
			}

			int medianSurfaceSize=0;
			if (surfaceSizes.size()%2==0 && surfaceSizes.size()>0)
			{
				medianSurfaceSize = (surfaceSizes[surfaceSizes.size()/2-1]+surfaceSizes[surfaceSizes.size()/2])/2;
			}
			else if (surfaceSizes.size()%2==1 && surfaceSizes.size()>0)
			{
				medianSurfaceSize = surfaceSizes[surfaceSizes.size()/2];
			}

			
			printf("----------------\n");
			printf("Total patches: %d\n",(int)patchSizes.size());
			printf("Total patch size: %d\n",totalPatchSize);
			printf("Median patch size: %d\n",medianPatchSize);
			printf("\n");
			printf("Total surfaces: %d\n",(int)surfaceSizes.size());
			printf("Total surface size: %d\n",totalSurfaceSize);
			printf("Median surface size: %d\n",medianSurfaceSize);
			printf("Number of large surfaces (>= %d voxels): %d\n",SIZE*SIZE/2,numLargeSurfaces);
			printf("----------------\n");
		}
};

void ExportAllGroups(const char *directory, std::vector<std::tuple<std::string,std::string,int> > &possibleNeighbours, std::vector<bool> &solution, RunStats &stats)
{
	int nextFreeGroupId = 0;
		
	std::set<int> groupIds;
	std::map<std::string,int> groupMap;
	
	// Work out which patches are in which sets
	for(int i = 0; i<solution.size(); i++)
	{		
		if (solution[i])
		{
			std::string f1 = std::get<0>(possibleNeighbours[i]);
			std::string f2 = std::get<1>(possibleNeighbours[i]);
			
			if (groupMap.count(f1) == 0 && groupMap.count(f2) == 0)
			{
				groupIds.insert(nextFreeGroupId);
				groupMap[f1] = groupMap[f2] = nextFreeGroupId++;
			}
			else if (groupMap.count(f1) == 0)
			{
				groupMap[f1] = groupMap[f2];
			}
			else if (groupMap.count(f2) == 0)
			{
				groupMap[f2] = groupMap[f1];
			}
			else if (groupMap[f2] != groupMap[f1])
			{
				int toRelabel = groupMap[f2];
				int newLabel = groupMap[f1];
				for(std::map<std::string,int>::iterator iter = groupMap.begin(); iter != groupMap.end(); ++iter)
				{
					std::string i = iter->first;
					if (groupMap[i]==toRelabel)
						groupMap[i] = newLabel;
				}		

				groupIds.erase(toRelabel);
			}
		}
	}
		
	for(int i : groupIds)
	{		
        std::vector<std::string> filesInGroup;

		for(std::map<std::string,int>::iterator iter = groupMap.begin(); iter != groupMap.end(); ++iter)
		{
			if (iter->second==i)
			{
			    filesInGroup.push_back(iter->first);
			}
		}

		// Sort files in descending order of size
		auto sortFilesLambda = [] (std::string const& s1, std::string const& s2) -> bool
		{
			return pointsets[s1].size() > pointsets[s2].size();
		};
    
		std::sort(filesInGroup.begin(), filesInGroup.end(), sortFilesLambda);  
			
		// Name of output corresponds to first file in list
		FILE *f = fopen((std::string(directory) + std::string("/output_jigsaw3/v")+filesInGroup[0].substr(1)).c_str(),"w");
		
		std::unordered_set<uint32_t> outputPointSet;
		
		for(const std::string& file : filesInGroup)
		{
			outputPointSet.merge(pointsets[file]);
		}	

 	    stats.surfaceSizes.push_back(outputPointSet.size());
		
        for(uint32_t u : outputPointSet)
		{
			int z = (u%PT_MULT)-1;
			int v = u/PT_MULT;
			int x = (v%PT_MULT)-1;
			int y = (v/PT_MULT)-1;
			
			fprintf(f,"%d,%d,%d\n",x,y,z);
		}
		
		fclose(f);
	}	
}

int main(int argc, char *argv[])
{
  if (argc != 2 && argc != 8)
  {
    printf("Usage: jigsaw3 <directory> [x1 y1 z1 x2 y2 z2]\n");
    return -1;
  }

  RunStats stats;
  
  if (argc==8)
  {
	for(int i = 0; i<2; i++)
	  for(int j = 0; j<3; j++)
	    planeVectors[i][j] = atoi(argv[j+i*3+argc-6]);
  }

  printf("Projection plane vectors normalised to length 1000:\n");
  for(int i = 0; i<2; i++)
  {
	double magnitude = sqrt((double)(planeVectors[i][0]*planeVectors[i][0]+planeVectors[i][1]*planeVectors[i][1]+planeVectors[i][2]*planeVectors[i][2]));
		
	for(int j = 0; j<3; j++)
	{
		planeVectors[i][j] /= (magnitude/1000);
		
		printf("%d ", planeVectors[i][j]);
	}
		
	printf("\n");
  }

  /* Cross product of the two plane vectors gives the normal vector */
  planeVectors[2][0] = (planeVectors[0][1]*planeVectors[1][2]-planeVectors[0][2]*planeVectors[1][1])/1000;
  planeVectors[2][1] = (planeVectors[0][2]*planeVectors[1][0]-planeVectors[0][0]*planeVectors[1][2])/1000;
  planeVectors[2][2] = (planeVectors[0][0]*planeVectors[1][1]-planeVectors[0][1]*planeVectors[1][0])/1000;
	
  printf("Normal vector normalised to length 1000:\n");
  printf("%d %d %d\n",planeVectors[2][0],planeVectors[2][1],planeVectors[2][2]);
  
  for(int j = 0; j<PROJECTION_SIZE; j++)
	for(int k = 0; k<PROJECTION_SIZE; k++)
	  projection[j][k] = PROJECTION_BLANK;

  printf("Loading volumes\n");

  if (argc == 2 || argc == 8)
  {
	std::string volumeDir(argv[1]);
	{
      DIR *dir;
	  struct dirent *ent;
	  if ((dir = opendir (argv[1])) != NULL)
	  {
		while ((ent = readdir (dir)) != NULL)
		{
          std::string file(ent->d_name);

		  /* As well as loading them, collect stats on total voxels, median patch size, how many patches */
          if (file[0]=='v' && file.substr(file.length()-4)==".csv")
		  {
			pointsets[std::string("x")+file.substr(1)] = LoadVolume( (volumeDir + "/" + file).c_str(),false);
			printf("%s %d\n",(std::string("x")+file.substr(1)).c_str(),(int)pointsets[std::string("x")+file.substr(1)].size());

			stats.patchSizes.push_back((int)pointsets[std::string("x")+file.substr(1)].size());
   		  }
		  else if (file[0]=='x' && file.substr(file.length()-4)==".csv")
		  {
		    files.push_back(file);
			extents[file] = LoadExtent( (volumeDir + "/" + file).c_str(),false);
		  }
		}
		closedir (dir);
	  } 
	  else
	  {
		/* could not open directory */
		printf("Could not open directory\n");
		return -2;
	  }
	}
  }
  
  printf("Finished loading %d volumes\n",(int)files.size());

  // Sort files in descending order of size
  auto sortFilesLambda = [] (std::string const& s1, std::string const& s2) -> bool
  {
     return pointsets[s1].size() > pointsets[s2].size();
  };
    
  std::sort(files.begin(), files.end(), sortFilesLambda);  

  // Populate 'plugs' for each volume - these are any points that a volume shares with any other volume
  for(int i = 0; i<files.size(); i++)
  {
	plugs[files[i]]=std::unordered_set<uint32_t>();
  }
  
  for(int i = 0; i<files.size(); i++)
  {  
    for(int j=0; j<files.size(); j++)
	{
		if (j!=i)
		{
	      std::unordered_set<uint32_t> tmp = UnorderedSetIntersection(pointsets[files[i]],pointsets[files[j]]);
		  plugs[files[i]].merge(tmp);
		  //plugs[files[j]].merge(tmp);
		}
    }
	
	printf("%s has %d plugs\n",files[i].c_str(),(int)plugs[files[i]].size());
  }
  
  for(const std::string &f : files)
    for(uint32_t u : pointsets[f])
	{
      int z = (u%PT_MULT)-1;
      int v = u/PT_MULT;
      int x = (v%PT_MULT)-1;
      int y = (v/PT_MULT)-1;

	  pointsets_xz[f].insert( (projectN(x,y,z,0)+1024)*2048+projectN(x,y,z,1)+1024 );
	}

  for(const std::string &f : files)
    for(uint32_t u : plugs[f])
	{
      int z = (u%PT_MULT)-1;
      int v = u/PT_MULT;
      int x = (v%PT_MULT)-1;
      int y = (v/PT_MULT)-1;

	  plugs_xz[f].insert( (projectN(x,y,z,0)+1024)*2048+projectN(x,y,z,1)+1024 );
	}
	
  std::vector<std::tuple<std::string,std::string,int> > possibleNeighbours;
  
  int numWithNoNeighbours = 0;
  // Work out wihch patches abut each other - any that do are neighbour candidates
  for(int i = 0; i<files.size(); i++)
  {
	bool anyNeighbours = false;
    for(int j=i+1; j<files.size(); j++)
	{
	  int a = Abutting(pointsets[files[i]],plugs[files[i]],pointsets_xz[files[i]],plugs_xz[files[i]],extents[files[i]],pointsets[files[j]],plugs[files[j]],pointsets_xz[files[j]],plugs_xz[files[j]],extents[files[j]]);
	  
	  if (a>0)
	  {
		possibleNeighbours.push_back(std::tuple(files[i],files[j],a));
		anyNeighbours = true;
	  }
	}
	
	if (!anyNeighbours)
		numWithNoNeighbours++;
  }
  
  printf("numWithNoNeighbours=%d\n",numWithNoNeighbours);
  
  // This vector stores the current state
  std::vector<bool> currentState(possibleNeighbours.size());

  printf("currentState size %d\n",(int)currentState.size());
  
  for(int i = 0; i<currentState.size(); i++)
    currentState[i]=false;

  //currentState[0]=true;
  int maxTemperature = 40;
  int temperature = maxTemperature;
  int counter = 0;
  
  int currentScore = EvaluateSolution(possibleNeighbours,currentState);
  
  while(temperature>0)
  {
	printf("Temperature %d, Score %d\n",temperature,currentScore);
	
    std::vector<bool> newState = currentState;
	
    for(int i = 0; i<newState.size(); i++)
	{
	  // rule for deciding whether to mutate
	  if (rand()%(1000*maxTemperature) < temperature)
	  {
        newState[i]=!newState[i];
	  }
    }
	
	int newScore = EvaluateSolution(possibleNeighbours,newState);
	
	// rule for deciding whether to accept new state
	if (newScore > currentScore || (1==0 && rand()%(5000*maxTemperature) < temperature))
	{
		currentState = newState;
		currentScore = newScore;
	}
	
	if (counter%1000==0)
	  temperature--;
  
    counter++;
  }

  ExportAllGroups(argv[1],possibleNeighbours,currentState,stats);

  printf("Finished\n");

  stats.Display();
  
  return 0;    
}
/*  
  
  // change it
  evaluate  
	
  std::set<std::string> filesProcessed;

  bool doneOuter = false;
  
  while (!doneOuter)
  {
    // done unless we find any matches
    doneOuter = true;

    // iterate through...  
    for(const std::string& prand : files)
	{
      bool reiterate = true;
      while (reiterate && filesProcessed.find(prand)==filesProcessed.end())
	  {
        reiterate = false;
      
        int bestScore = 0;
        std::string bestFile;
      
        // does anything match? if so then pick the best match
        for(const std::string &p : files)
          if (filesProcessed.find(p)==filesProcessed.end() && p != prand)
		  {
            int score = NeighbourTest(prand.c_str(),p.c_str(),pointsets[prand],pointsets_xz[prand],extents[prand],pointsets[p],pointsets_xz[p],extents[p]);
            if (score>bestScore)
			{
              bestScore = score;
              bestFile = p;
			}
          }
		  
        if (bestScore > 0)
		{
          reiterate = true;
          doneOuter = false;

          printf("Merging %s and %s\n",prand.c_str(),bestFile.c_str());
        
          filesProcessed.insert(bestFile);
            
          // Merge bestFile with prand
          pointsets[prand].merge(pointsets[bestFile]);
          pointsets_xz[prand].merge(pointsets_xz[bestFile]);
          extents[prand] = UpdateExtent(extents[prand],extents[bestFile]);

          // Remove bestFile from the dictionaries 
          pointsets.erase(bestFile);
          pointsets_xz.erase(bestFile);
          extents.erase(bestFile);
        }
	  }
	}
  }
  
  // Now output everything left as distinct surfaces
  int outputCount = 0;
  for(const std::string &p : files)
    if (filesProcessed.find(p)==filesProcessed.end())
	{
	  FILE *f = fopen( (std::string(argv[1]) + std::string("/output/x")+p.substr(1)).c_str(),"w");

      fprintf(f,"%d,%d,%d,%d,%d,%d\n",extents[p][0],extents[p][1],extents[p][2],extents[p][3],extents[p][4],extents[p][5]);

	  fclose(f);
	  
      f = fopen( (std::string(argv[1]) + std::string("/output/v")+p.substr(1)).c_str(),"w");

 	  stats.surfaceSizes.push_back(pointsets[p].size());
	  
	  for(uint32_t u : pointsets[p])
	  {
          int z = (u%PT_MULT)-1;
          int v = u/PT_MULT;
          int x = (v%PT_MULT)-1;
          int y = (v/PT_MULT)-1;

          fprintf(f,"%d,%d,%d\n",x,y,z);
      }
      outputCount += 1;
    }
	
  printf("Finished\n");

  stats.Display();
  
  return 0;  
}
*/