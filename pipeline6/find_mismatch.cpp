/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculaneum Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 
#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#include "parameters.h"

#define ROUND(x) (((x)-(int)(x))<0.5?(int)(x):((int)(x))+1)

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

std::vector<std::tuple<int,float,float,float,float,int>> patchPositions; 

// what resolution to use for points in rendered?
#define RESCALE 4
std::map<std::tuple<int,int>,std::tuple<int,float,float,float> > rendered;

#define D_SIZE_X 2000
#define D_SIZE_Y 2000
#define D_OFF_X 1000
#define D_OFF_Y 1000
uint8_t distances[D_SIZE_X][D_SIZE_Y];

// If two apparentl overlapping points are further apart than this, something has gone wrong
#define DISTANCE_THRESHHOLD 40

std::set<std::tuple<int,int> > mismatches;

float Distance(float x, float y, float z)
{
	return sqrt(x*x+y*y+z*z);
}

void render(char *dirname)
{
	float maxDistance = 0;
	char filename[300];
	
	for(auto const &patch : patchPositions)
	{
		int patchNum = std::get<0>(patch);
		// patch position and angle
		float xp = std::get<1>(patch);
		float yp = std::get<2>(patch);
		float ca = std::get<3>(patch);
		float sa = std::get<4>(patch);
		int flip = std::get<5>(patch);
				
		gridPointStruct p;

		//if (patchNum != 1150 && patchNum != 5 && patchNum != 1511 && patchNum != 1508) continue;
		
		sprintf(filename,"%s/patch_%d.bin",dirname,patchNum);
		FILE *f = fopen(filename,"r");

		if (f)
		{
			fseek(f,0,SEEK_END);
			long fsize = ftell(f);
			fseek(f,0,SEEK_SET);
  
			// input in x,y,z order 
			while(ftell(f)<fsize)
			{
				fread(&p,sizeof(p),1,f);
				float x,y,px,py,pz;
				x=p.x;y=p.y;px=p.px;py=p.py;pz=p.pz;
				
				x=x*QUADMESH_SIZE;
				y=y*QUADMESH_SIZE;
				
				if (flip)
					x = -x;
				
				// transform to get position of this patch point
				float xrd = x*ca+y*sa + xp;
				float yrd = y*ca-x*sa + yp;

				int xrdi = ROUND(xrd/RESCALE);
				int yrdi = ROUND(yrd/RESCALE);
					  
				if (rendered.count(std::tuple<int,int>(xrdi,yrdi))==0)
				{
				  rendered[std::tuple<int,int>(xrdi,yrdi)] = std::tuple<int,float,float,float>(patchNum,px,py,pz);
				}
                else
				{
					int ePatchNum = std::get<0>(rendered[std::tuple<int,int>(xrdi,yrdi)]);
					
					float ex = std::get<1>(rendered[std::tuple<int,int>(xrdi,yrdi)]);
					float ey = std::get<2>(rendered[std::tuple<int,int>(xrdi,yrdi)]);
					float ez = std::get<3>(rendered[std::tuple<int,int>(xrdi,yrdi)]);
					
					float distance = Distance(ex-px,ey-py,ez-pz); 
					
                    distances[xrdi+D_OFF_X][yrdi+D_OFF_Y] = distance*7;
					
                     //printf("%d %d %f\n",patchNum,ePatchNum,distance);							
					
					if (distance > maxDistance) maxDistance = distance;
					
					if (distance > DISTANCE_THRESHHOLD)
					{
						if (mismatches.count(std::pair<int,int>(patchNum,ePatchNum))==0)
						{
							mismatches.insert(std::pair<int,int>(patchNum,ePatchNum));
							printf("%d,%d : %f\n",patchNum,ePatchNum,distance);	
						}
					}
				}
			}
			
			fclose(f);
		}
	}			
	
	printf("Max distance:%f\n",maxDistance);
}

void RenderDistances(void)
{
		TIFF *tif = TIFFOpen("d:/temp/distances.tif","w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, D_SIZE_X); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, D_SIZE_Y); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(D_SIZE_X);
			for(row=0; row<D_SIZE_Y;row++)
			{
				for(int i=0; i<D_SIZE_X; i++)
				{
				   ((uint8_t *)buf)[i] = distances[i][row];
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: find_mismatch <patch dir> <patch positions>\n");
		printf("Check for patches with similar quadmesh coords but different volume coords\n");
		return -1;
	}
	
	FILE * f = fopen(argv[2],"r");
	
	if (f)
	{
	  int patchNum,flip;
      float x,y,angle;	 
      bool first = true;	  
	  while(fscanf(f,"%d %f %f %f %d",&patchNum,&x,&y,&angle,&flip)==5)
	  {
		  if (!first) angle -= 3.1415926535/2.0;
		  patchPositions.push_back(std::tuple<int,float,float,float,float,int>(patchNum,x,y,cos(angle),sin(angle),flip));
		  first = false;
	  }
	  
	  render(argv[1]);
	
	  fclose(f);
	}
	
	RenderDistances();
	
	return 0;
}

