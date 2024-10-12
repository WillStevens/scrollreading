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

#include <unordered_set>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>

#define PT_MULT 1024

std::vector<std::string> fibreFiles;
std::map<std::string,std::vector<int16_t> > fibreExtents;
std::map<std::string,std::unordered_set<uint32_t> > fibrePointsets;

std::vector<std::string> files;
std::map<std::string,std::vector<int16_t> > extents;
std::map<std::string,std::unordered_set<uint32_t> > pointsets;

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
	
	return extent;
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
	
	return volume;
}

std::unordered_set<uint32_t> UnorderedSetIntersection(std::unordered_set<uint32_t> &a,std::unordered_set<uint32_t> &b)
{
	std::unordered_set<uint32_t> r;
	
	for(uint32_t x : a)
	{
	   printf("in here\n");
	   if (b.find(x)!=b.end())
			r.insert(x);
	}
	
	return r;
}

int NeighbourTest(const char *file1, const char *file2, std::unordered_set<uint32_t> &ps1
                                                      , std::unordered_set<uint32_t> &ps1_xz
													  , std::vector<int16_t> &e1
													  , std::unordered_set<uint32_t> &ps2
                                                      , std::unordered_set<uint32_t> &ps2_xz
													  , std::vector<int16_t> &e2
													  )
{
  int xmin1=e1[0], ymin1=e1[1], zmin1=e1[2], xmax1=e1[3], ymax1=e1[4], zmax1=e1[5];
  int xmin2=e2[0], ymin2=e2[1], zmin2=e2[2], xmax2=e2[3], ymax2=e2[4], zmax2=e2[5];
  
  bool overlap = false;
  
  if (((xmin2 >= xmin1 && xmin2 <= xmax1) || (xmax2 >= xmin1 && xmax2 <= xmax1)) &&
      ((ymin2 >= ymin1 && ymin2 <= ymax1) || (ymax2 >= ymin1 && ymax2 <= ymax1)) &&
      ((zmin2 >= zmin1 && zmin2 <= zmax1) || (zmax2 >= zmin1 && zmax2 <= zmax1)))
    overlap = true;
   
  if (((xmin1 >= xmin2 && xmin1 <= xmax2) || (xmax1 >= xmin2 && xmax1 <= xmax2)) &&
      ((ymin1 >= ymin2 && ymin1 <= ymax2) || (ymax1 >= ymin2 && ymax1 <= ymax2)) &&
      ((zmin1 >= zmin2 && zmin1 <= zmax2) || (zmax1 >= zmin2 && zmax1 <= zmax2)))
    overlap = true;

  if (!overlap)
    return 0;
  printf("NT 2: %s %s %d %d\n",file1,file2,(int)ps1.size(),(int)ps2.size());
    
  std::unordered_set<uint32_t> common = UnorderedSetIntersection(ps1,ps2);
                
  std::unordered_set<uint32_t> common_xz = UnorderedSetIntersection(ps1_xz,ps2_xz);
    
  unsigned l1 = ps1.size();
  unsigned l2 = ps2.size();
  
  unsigned lmin = (l1<l2)?l1:l2;
      
  /* These two tests aim to limit the number of intersections that we end up with
   * Only join if the common boundary is more than a value close to the square root of the approx area of the smallest patch, and if the number of intersection voxels is no more than a small constant
   * times the boundary size.
   */
  bool r = (common.size()*common.size()>lmin/4 && common_xz.size() <= common.size());
  
  int fibreBonus = 0;
  
  if (common.size()>0)
  {
    // If the two pointsets have a fibre in common, this increases the score
    std::set<std::string> ps1Fibres;
    std::set<std::string> ps2Fibres;
    for(const std::string &fibreFile : fibreFiles)
	{
      if (UnorderedSetIntersection(fibrePointsets[fibreFile],ps1).size()>0)
         ps1Fibres.insert(fibreFile);
      if (UnorderedSetIntersection(fibrePointsets[fibreFile],ps2).size()>0)
         ps2Fibres.insert(fibreFile);
	}
	
	int fibreBonus = 0;
	
    std::set<std::string> intersect;
    std::set_intersection(ps1Fibres.begin(), ps1Fibres.end(), ps2Fibres.begin(), ps2Fibres.end(),
                 std::inserter(intersect, intersect.begin()));
	if (intersect.size() > 0)
		fibreBonus = 200;
	
    printf("NT %s: l1=%d l2=%d len(common)=%d lmin=%d len(common_xz)=%d fibreBonus=%d (%s,%s)",r?"true":"false",l1,l2,(int)common.size(),lmin,
	(int)common_xz.size(),fibreBonus,file1,file2);
  }
  
  if (r)
    return common.size() + fibreBonus;
  else
    return 0;
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

int main(int argc, char *argv[])
{
  if (argc != 2 && argc != 3)
  {
    printf("Usage: jigsaw2 <directory> [<fiber-directory>]");
    return -1;
  }
  
  printf("Loading volumes and fibers\n");

  if (argc == 3)
  {
	std::string fibreDir(argv[2]);
	{
      DIR *dir;
	  struct dirent *ent;
	  if ((dir = opendir (argv[2])) != NULL)
	  {
		while ((ent = readdir (dir)) != NULL)
		{
          std::string file(ent->d_name);

          if (file[0]=='v' && file.substr(file.length()-4)==".csv")
		  {
			fibrePointsets[std::string("x")+file.substr(1)] = LoadVolume( (fibreDir + std::string("/") + file).c_str(),false);
			printf("%s %d\n",file.c_str(),(int)fibrePointsets[std::string("x")+file.substr(1)].size());
		  }
		  else if (file[0]=='x' && file.substr(file.length()-4)==".csv")
		  {
		    fibreFiles.push_back(file);
			fibreExtents[file] = LoadExtent( (fibreDir + std::string("/") + file).c_str(),false);
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

          if (file[0]=='v' && file.substr(file.length()-4)==".csv")
		  {
			pointsets[std::string("x")+file.substr(1)] = LoadVolume( (volumeDir + "/" + file).c_str(),false);
			printf("%s %d\n",(std::string("x")+file.substr(1)).c_str(),(int)pointsets[std::string("x")+file.substr(1)].size());
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
  
  printf("Finished loading volumes and fibers\n");

  // Sort files in descending order of size
  auto sortFilesLambda = [] (std::string const& s1, std::string const& s2) -> bool
  {
     return pointsets[s1].size() > pointsets[s2].size();
  };
    
  std::sort(files.begin(), files.end(), sortFilesLambda);  
  
  std::map<std::string,std::unordered_set<uint32_t> > pointsets_xz;

  std::map<std::string,std::unordered_set<uint32_t> > pointsets;

  for(const std::string &f : files)
    for(uint32_t x : pointsets[f])
	  pointsets_xz[f].insert(x%(PT_MULT*PT_MULT));

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
            int score = NeighbourTest(p.c_str(),prand.c_str(),pointsets[prand],pointsets_xz[prand],extents[prand],pointsets[p],pointsets_xz[p],extents[p]);
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
	  FILE *f = fopen( (std::string(argv[1]) + std::string("/output/x")+std::to_string(outputCount)+std::string(".csv")).c_str(),"w");

      fprintf(f,"%d,%d,%d,%d,%d,%d\n",extents[p][0],extents[p][1],extents[p][2],extents[p][3],extents[p][4],extents[p][5]);

	  fclose(f);
	  
      f = fopen( (std::string(argv[1]) + std::string("/output/v")+std::to_string(outputCount)+std::string(".csv")).c_str(),"w");
	  
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

  return 0;  
}