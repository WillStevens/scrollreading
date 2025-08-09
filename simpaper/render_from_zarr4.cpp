/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#include "../zarrlike/zarr_1.c"

#include "bigpatch.cpp"

#define ROUND(x) ((x)-(int)(x)<0.5?(int)(x):((int)(x))+1)

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

int minx=-1,maxx=-1,miny=-1,maxy=-1;
int SHEET_SIZE_X=0;
int SHEET_SIZE_Y=0;

#define FILENAME_LENGTH 300
#define FOLDERNAME_LENGTH 256
char targetDir[FOLDERNAME_LENGTH]="";
char imageDir[FOLDERNAME_LENGTH]="";
unsigned zOffset=0;

unsigned char **rendered;

void render(char *fname, int maskOnly)
{
	float xo,yo,zo;
	int x,y,z;
	int fnameLength = strlen(fname);
	
	bool isBP = fname[fnameLength-1]=='p' && fname[fnameLength-2]=='b';
	bool isCSV = fname[fnameLength-1]=='v' && fname[fnameLength-2]=='s' && fname[fnameLength-3]=='c';
		
    //initialize the volume
    ZARR_1 *volumeZarr = NULL;

    if (!maskOnly)
	{
        volumeZarr = ZARROpen_1("d:/zarrs/54keV_7.91um_Scroll1A.zarr/0/");
	}
	
	BigPatch *bp = NULL;
	std::vector<chunkIndex> bpChunks;
	
	if (isBP)
	{
		bp = OpenBigPatch(fname);
		bpChunks = GetAllPatchChunks(bp);
	}
	else
	{
		// dummy
		bpChunks.push_back(chunkIndex(0,0,0));
	}
	
	float xr,yr;

	bool first = true;
	for(int pass = 0; pass<2; pass++)
	{
		for(auto const &i : bpChunks)
		{
			//printf("Pass: %d, chunk: %d.%d.%d\n",pass,std::get<2>(i),std::get<1>(i),std::get<0>(i));
			std::vector<gridPoint> gridPoints;
			if (isBP)
			{
				ReadPatchPoints(bp,i,gridPoints);
			}
			else
			{
				FILE *f = fopen(fname,"r");
				
				if (isCSV)
				{
					while(fscanf(f,"%f,%f,%f,%f,%f\n",&xr,&yr,&xo,&yo,&zo)==5)
						gridPoints.push_back(gridPoint(xr,yr,xo,yo,zo,0));
				}
				else
				{
					gridPointStruct p;
					fseek(f,0,SEEK_END);
					long fsize = ftell(f);
					fseek(f,0,SEEK_SET);
  
					// input in x,y,z order 
					while(ftell(f)<fsize)
					{
						fread(&p,sizeof(p),1,f);
						float x,y,px,py,pz;
						x=p.x;y=p.y;px=p.px;py=p.py;pz=p.pz;
						gridPoints.push_back(gridPoint(x,y,px,py,pz,0));
					}
				}
				
				fclose(f);
			}
			
			for(auto const &gp : gridPoints)
			{
				xr = std::get<0>(gp);
				yr = std::get<1>(gp);
				xo = std::get<2>(gp);
				yo = std::get<3>(gp);
				zo = std::get<4>(gp);
				
				x = (int)xo;
				y = (int)yo;
				z = (int)zo;

				if (1)
				{
					for(int xo=ROUND(xr)+(maskOnly?-1:0); xo<=ROUND(xr)+(maskOnly?1:0); xo++)
					for(int yo=ROUND(yr)+(maskOnly?-1:0); yo<=ROUND(yr)+(maskOnly?1:0); yo++)
					{
						if (pass == 1)
						{				
                            if (yo<miny || xo<minx || xo-minx>=SHEET_SIZE_X || yo-miny>=SHEET_SIZE_Y)
							{
								printf("%d,%d,%d,%d\n",xo,yo,minx,miny);
							}
							
							rendered[yo-miny][xo-minx] = maskOnly?255:ZARRRead_1(volumeZarr,z,y,x);
						}
						else
						{
							if (xo>maxx || first) maxx=xo;
							if (xo<minx || first) minx=xo;
							if (yo>maxy || first) maxy=yo;
							if (yo<miny || first) miny=yo;
							first = false;
						}							
					}
				}
			}
		}
		
		if (pass == 0)
		{
			SHEET_SIZE_X = maxx-minx+1;
			SHEET_SIZE_Y = maxy-miny+1;
			
			rendered = (unsigned char **)malloc(sizeof(unsigned char *)*SHEET_SIZE_Y);
			
			for(int y=0; y<SHEET_SIZE_Y; y++)
			{
				rendered[y] = (unsigned char *)malloc(sizeof(unsigned char)*SHEET_SIZE_X);
				for(int x=0; x<SHEET_SIZE_X; x++)
					rendered[y][x] = 0;
			}
		}	
    }
    	
	if (isBP) CloseBigPatch(bp);   
	
	//printf("About to render\n");
	
	if (!maskOnly)
        ZARRClose_1(volumeZarr);
	
	if (1==1)
	{
		char tiffName[FILENAME_LENGTH];
		strcpy(tiffName,fname);
		/* Now write tiff file */
		int l = strlen(tiffName);
		if (!strcmp(tiffName+l-3,".bp"))
		  sprintf(tiffName+l-3,".tif");
		else if (!strcmp(tiffName+l-4,".csv"))
		  sprintf(tiffName+l-4,".tif");	  
		else if (!strcmp(tiffName+l-4,".bin"))
		  sprintf(tiffName+l-4,".tif");	  
		else
		  return;
		

		TIFF *tif = TIFFOpen(tiffName,"w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, SHEET_SIZE_X); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, SHEET_SIZE_Y); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(SHEET_SIZE_X);
			for(row=0; row<SHEET_SIZE_Y;row++)
			{
				for(int i=0; i<SHEET_SIZE_X; i++)
				{
				   ((uint8_t *)buf)[i] = rendered[row][i];
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

	}
}

int main(int argc, char *argv[])
{
	if (argc != 2 && argc != 3)
	{
		printf("Usage: render_from_zarr4 <bigpatch or csv or bin> [1 if mask only]\n");
		printf("Render from a zarr. If mak only then output a white dilated mask on black background");
		return -1;
	}
	
	render(argv[1],argc==3 && argv[2][0]=='1');
	
	printf("%d %d\n",minx,miny);
	
	return 0;
}

