/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 
#include <unordered_map>

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "tiffio.h"

#include "zarr_4.c"

#include "bigpatch.h"

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
unsigned char **mask;

std::unordered_map<int,std::tuple<float,float,float,float>> patchPosLookup; 

void render(char *zname,char *fname, char *colourFile, int maskOnly)
{
	float xo,yo,zo;
	int x,y,z;
	int patch;
	int fnameLength = strlen(fname);
	
	bool isBP = fname[fnameLength-1]=='p' && fname[fnameLength-2]=='b';
	bool isCSV = fname[fnameLength-1]=='v' && fname[fnameLength-2]=='s' && fname[fnameLength-3]=='c';
		
    //initialize the volume
    ZARR_4 *volumeZarr = NULL;

	printf("Here XX\n");

    if (!maskOnly)
	{
        volumeZarr = ZARROpen_4(zname);
	}
	printf("Here Y\n");
	
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

	printf("Here A\n");
	bool first = true;
	for(int pass = 0; pass<2; pass++)
	{
		for(auto const &i : bpChunks)
		{
			//printf("Pass: %d, chunk: %d.%d.%d\n",pass,std::get<2>(i),std::get<1>(i),std::get<0>(i));
			std::vector<gridPoint> gridPoints;
			std::vector<std::tuple<int,int,int>> colours;
			if (isBP)
			{
				ReadPatchPoints(bp,i,gridPoints);
			}
			else
			{
				FILE *f = fopen(fname,"r");
				FILE *cf = NULL;
			
				int r,g,b;
				
				if (colourFile)
					cf = fopen(colourFile,"r");
				
				if (isCSV)
				{
					while(fscanf(f,"%f,%f,%f,%f,%f\n",&xr,&yr,&xo,&yo,&zo)==5)
					{
						gridPoints.push_back(gridPoint(xr,yr,xo,yo,zo,0));
						if (cf)
						{
							fscanf(cf,"%d,%d,%d\n",&r,&g,&b);
							colours.push_back({r,g,b});
						}
					}
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
						
						if (cf)
						{
							fscanf(cf,"%d,%d,%d\n",&r,&g,&b);
							colours.push_back({r,g,b});
						}
					}
				}
				
				fclose(cf);
				fclose(f);
			}

	        printf("Here B\n");
			
			int index = 0;
			for(auto const &gp : gridPoints)
			{
				xr = std::get<0>(gp);
				yr = std::get<1>(gp);
				xo = std::get<2>(gp);
				yo = std::get<3>(gp);
				zo = std::get<4>(gp);
				patch = std::get<5>(gp);
				
				if (patchPosLookup.count(patch)==0)
					continue;
				
				x = (int)xo;
				y = (int)yo;
				z = (int)zo;

				// transform xr,yr
				std::tuple<float,float,float,float> patchPos = patchPosLookup[patch];
				float cosangle = std::get<2>(patchPos);
				float sinangle = std::get<3>(patchPos);
				float xrd = xr*cosangle+yr*sinangle + std::get<0>(patchPos);
				float yrd = yr*cosangle-xr*sinangle + std::get<1>(patchPos);
/*				
				if (patch==0)
				{
					printf("ca: %f, sa: %f\n",cosangle,sinangle);
					printf("xo: %f, yo: %f\n",std::get<0>(patchPos),std::get<1>(patchPos));
					printf("xr: %f, xr: %f\n",xr,yr);
					printf("xrd: %f, xrd: %f\n",xrd,yrd);					
				}
*/				
				if (1)
				{
					for(int xo=ROUND(xrd)+(maskOnly?-1:0); xo<=ROUND(xrd)+(maskOnly?1:0); xo++)
					for(int yo=ROUND(yrd)+(maskOnly?-1:0); yo<=ROUND(yrd)+(maskOnly?1:0); yo++)
					{
						if (pass == 1)
						{				
                            if (yo<miny || xo<minx || xo-minx>=SHEET_SIZE_X || yo-miny>=SHEET_SIZE_Y)
							{
								printf("%d,%d,%d,%d\n",xo,yo,minx,miny);
							}
							uint16_t v;
							v = ZARRRead_4(volumeZarr,z,y,x);
							if (v==0)
							{
								printf("x,y,z=%d,%d,%d is 0\n",x,y,z);
							}
														
							unsigned char value = (v>>8);
							
							int r=255,g=255,b=255;
							
							if (index<colours.size())
							{
								r = std::get<0>(colours[index]);
								g = std::get<1>(colours[index]);
								b = std::get<2>(colours[index]);
							}
							
							int rr,rg,rb;
							{
								rr = maskOnly?255:r*value/255;
								rg = maskOnly?255:g*value/255;
								rb = maskOnly?255:b*value/255;
							}
							
							if (z%100<8)
							{
								rr += 64; 
								rg += 64; 
								rb += 64; 
							}

							rr += (x%200)/2-50;
							rg += (x%200)/2-50;
							rb += (x%200)/2-50;
							
							if (rr>255) rr=255;
							if (rg>255) rg=255;
							if (rb>255) rb=255;
							
							if (rr<0) rr=0;
							if (rg<0) rg=0;
							if (rb<0) rb=0;
							
							{
								rendered[yo-miny][(xo-minx)*3+0] = maskOnly?255:rr;
								rendered[yo-miny][(xo-minx)*3+1] = maskOnly?255:rg;
								rendered[yo-miny][(xo-minx)*3+2] = maskOnly?255:rb;
							}

							mask[yo-miny][xo-minx] = 255;
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
				
				index++;
			}
		}
		
		if (pass == 0)
		{
			SHEET_SIZE_X = maxx-minx+1;
			SHEET_SIZE_Y = maxy-miny+1;
			
			rendered = (unsigned char **)malloc(sizeof(unsigned char *)*SHEET_SIZE_Y);
			mask = (unsigned char **)malloc(sizeof(unsigned char *)*SHEET_SIZE_Y);
			
			for(int y=0; y<SHEET_SIZE_Y; y++)
			{
				rendered[y] = (unsigned char *)malloc(sizeof(unsigned char)*SHEET_SIZE_X*3);
				mask[y] = (unsigned char *)malloc(sizeof(unsigned char)*SHEET_SIZE_X);
				for(int x=0; x<SHEET_SIZE_X; x++)
				{
					mask[y][x] = 0;
					rendered[y][x*3+0] = 0;
					rendered[y][x*3+1] = 0;
					rendered[y][x*3+2] = 0;
				}
			}
		}	
    }
    	
	if (isBP) CloseBigPatch(bp);   
	
	//printf("About to render\n");
	
	if (!maskOnly)
        ZARRClose_4(volumeZarr);
	
	// tiffout = 0 : image
	// tiffout = 1 : mask
	for(int tiffout = 0; tiffout<2; tiffout++)
	{
		char tiffName[FILENAME_LENGTH];
		strcpy(tiffName,fname);
		/* Now write tiff file */
		int l = strlen(tiffName);
		if (!strcmp(tiffName+l-3,".bp"))
		  sprintf(tiffName+l-3,tiffout?".tifm":".tif");
		else if (!strcmp(tiffName+l-4,".csv"))
		  sprintf(tiffName+l-4,tiffout?".tifm":".tif");	  
		else if (!strcmp(tiffName+l-4,".bin"))
		  sprintf(tiffName+l-4,tiffout?".tifm":".tif");	  
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
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, SHEET_SIZE_Y);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
						
			buf = _TIFFmalloc(SHEET_SIZE_X*3);
			for(row=0; row<SHEET_SIZE_Y;row++)
			{
				for(int i=0; i<SHEET_SIZE_X; i++)
				{
				   ((uint8_t *)buf)[3*i+0] = (tiffout?mask:rendered)[row][3*i+0];
				   ((uint8_t *)buf)[3*i+1] = (tiffout?mask:rendered)[row][3*i+1];
				   ((uint8_t *)buf)[3*i+2] = (tiffout?mask:rendered)[row][3*i+2];
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
	if (argc != 4 && argc != 5 && argc != 6 && argc != 7 || (argc>=6 && (strlen(argv[4]) != 2 || strcmp(argv[4],"-c"))) )
	{
		printf("Usage: render_from_zarr6 <zarr> <bigpatch or csv or bin> <patch positions> [-c  <colours>] [1 if mask only]\n");
		printf("Render from a zarr. If mak only then output a white dilated mask on black background");
		return -1;
	}

	char *colourFile = NULL;
	
	if (argc>=6)
		colourFile = argv[5];
	
	FILE * f = fopen(argv[3],"r");
	
	if (f)
	{
	  int patchNum;
      float x,y,angle;	  
	  while(fscanf(f,"%d %f %f %f",&patchNum,&x,&y,&angle)==4)
	  {
		  angle -= 3.1415926535/2.0;
		  patchPosLookup[patchNum] = std::tuple<float,float,float,float>(x,y,cos(angle),sin(angle));
	  }
	  
	  render(argv[1],argv[2],colourFile,(argc==5 || argc==7) && argv[argc-1][0]=='1');
	
	  fclose(f);
	  printf("%d %d\n",minx,miny);
	}
	else
	{
      patchPosLookup[0] = std::tuple<float,float,float,float>(0,0,1,0);
	  printf("Here Z\n");

	  render(argv[1],argv[2],colourFile,(argc==5 || argc==7) && argv[argc-1][0]=='1');
	
	  printf("%d %d\n",minx,miny);
	}
	
	return 0;
}

