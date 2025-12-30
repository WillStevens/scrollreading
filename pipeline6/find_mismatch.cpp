/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculaneum Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 
#include <vector>
#include <unordered_map>
#include <set>

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#include "parameters.h"

#undef OUTPUT_DISTANCE_TIF

#define ROUND(x) (((x)-(int)(x))<0.5?(int)(x):((int)(x))+1)

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

std::vector<std::tuple<int,float,float,float,float,int>> patchPositions; 

// what resolution to use for points in rendered?
#define RESCALE 4
std::unordered_map<uint64_t,std::tuple<int,float,float,float> > rendered;

#define R_ARRAY_SIZE 4000
int rendered_i[R_ARRAY_SIZE][R_ARRAY_SIZE];
int rendered_f[R_ARRAY_SIZE][R_ARRAY_SIZE][3];

#define MAKE_KEY(x,y) ((((uint64_t)x)<<32)+(uint64_t)y)

#ifdef OUTPUT_DISTANCE_TIF

#define D_SIZE_X 2000
#define D_SIZE_Y 2000
#define D_OFF_X 1000
#define D_OFF_Y 1000
uint8_t distances[D_SIZE_X][D_SIZE_Y];

#endif

// If two apparently overlapping points are further apart than this, something has gone wrong
int DISTANCE_THRESHHOLD = 40;

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
				
		gridPointStruct *p;

		//if (patchNum != 1150 && patchNum != 5 && patchNum != 1511 && patchNum != 1508) continue;
		
		sprintf(filename,"%s/patch_%d.bin",dirname,patchNum);
		FILE *f = fopen(filename,"r");

		if (f)
		{
			fseek(f,0,SEEK_END);
			long fsize = ftell(f);
			fseek(f,0,SEEK_SET);
  
            gridPointStruct *allPoints = (gridPointStruct *)malloc(fsize);
			
			int numPoints = fsize/sizeof(gridPointStruct);
			fread(allPoints,sizeof(gridPointStruct),numPoints,f);
	
            fprintf(stderr,"Loaded %d points for patch %d\n",numPoints,patchNum);	
			// input in x,y,z order 
			for(p=allPoints; p<allPoints+numPoints; p++)
			{
				float x,y,px,py,pz;
				x=p->x;y=p->y;px=p->px;py=p->py;pz=p->pz;
				
				//printf("%f %f %f %f %f\n",x,y,px,py,pz);
				
				x=x*QUADMESH_SIZE;
				y=y*QUADMESH_SIZE;
				
				if (flip)
					x = -x;
				
				// transform to get position of this patch point
				float xrd = x*ca+y*sa + xp;
				float yrd = y*ca-x*sa + yp;

				int xrdi = ROUND(xrd/RESCALE);
				int yrdi = ROUND(yrd/RESCALE);
#if 0					  
				if (rendered.count(MAKE_KEY(xrdi,yrdi))==0)
				{
				  rendered[MAKE_KEY(xrdi,yrdi)] = std::tuple<int,float,float,float>(patchNum,px,py,pz);
				}
                else
				{
					int ePatchNum = std::get<0>(rendered[MAKE_KEY(xrdi,yrdi)]);
					
					float ex = std::get<1>(rendered[MAKE_KEY(xrdi,yrdi)]);
					float ey = std::get<2>(rendered[MAKE_KEY(xrdi,yrdi)]);
					float ez = std::get<3>(rendered[MAKE_KEY(xrdi,yrdi)]);
#else
				if (rendered_i[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2]==0)
				{
				  rendered_i[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2] = patchNum+1;
				  rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][0] = px;
				  rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][1] = py;
				  rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][2] = pz;

#ifdef OUTPUT_DISTANCE_TIF
                  distances[xrdi+D_OFF_X][yrdi+D_OFF_Y] = 1; // don't use zero, because that means 'empty'
#endif
				}
                else
				{
					int ePatchNum = rendered_i[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2]-1;
					
					float ex = rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][0];
					float ey = rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][1];
					float ez = rendered_f[xrdi+R_ARRAY_SIZE/2][yrdi+R_ARRAY_SIZE/2][2];

#endif	
					float distance = Distance(ex-px,ey-py,ez-pz); 
					
#ifdef OUTPUT_DISTANCE_TIF
                    distances[xrdi+D_OFF_X][yrdi+D_OFF_Y] = (distance*5)/6;
#endif					
                    //fprintf(stderr,"%d %d %f\n",patchNum,ePatchNum,distance);							
					
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
			
			free(allPoints);
			fclose(f);
		}
		else
	      fprintf(stderr,"Unable to open file for patch %d\n",patchNum);
	}			
	
	printf("Max distance:%f\n",maxDistance);
}

#ifdef OUTPUT_DISTANCE_TIF
void RenderDistances(void)
{
		TIFF *tif = TIFFOpen("d:/s4_explore/distances.tif","w");
		
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
				   if (distances[i][row]!=0)
				     ((uint8_t *)buf)[i] = distances[i][row];
				   else
				     ((uint8_t *)buf)[i] = 255;
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

}
#endif

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		printf("Usage: find_mismatch <patch dir> <patch positions> <distance threshhold>\n");
		printf("Check for patches with similar quadmesh coords but different volume coords\n");
		return -1;
	}
	
	DISTANCE_THRESHHOLD = atoi(argv[3]);
	
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
	
#ifdef OUTPUT_DISTANCE_TIF
	RenderDistances();
#endif	
	return 0;
}

