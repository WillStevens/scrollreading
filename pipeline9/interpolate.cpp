#include <stdio.h>
#include <stdlib.h>

#define DOWNSCALE 4

#define SHEET_SIZE_X 3000
#define SHEET_SIZE_Y 3000

float paperPos[SHEET_SIZE_X][SHEET_SIZE_Y][3];
bool active[SHEET_SIZE_X][SHEET_SIZE_Y];

float outputPaperPos[SHEET_SIZE_X*DOWNSCALE][SHEET_SIZE_Y*DOWNSCALE][3];
bool activeOutput[SHEET_SIZE_X*DOWNSCALE][SHEET_SIZE_Y*DOWNSCALE];

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPoint;

bool LoadSheet(char *filename, int xoffset, int yoffset, float &minux, float &minuy)
{
  int numPoints = 0;
  
  for(int x = 0; x<SHEET_SIZE_X; x++)
  for(int y = 0; y<SHEET_SIZE_Y; y++)
  {
	active[x][y] = false; 
  }
  
  gridPoint p;
    
  for(int pass=0; pass<2; pass++)
  {
	  numPoints = 0;
	  
	  if (pass==1)
	  {
		  printf("minux,minuy=%f,%f\n",minux,minuy);
	  }
	  
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
		
		  p.x -= xoffset;
		  p.y -= yoffset;
		
		  if (pass == 0)
		  {
			  if (numPoints==0 || p.x<minux) minux = p.x;
			  if (numPoints==0 || p.y<minuy) minuy = p.y;
		  }
		  else if (p.x-minux>=0 && p.x-minux<SHEET_SIZE_X && p.y-minuy>=0 && p.y-minuy<SHEET_SIZE_Y)
		  {
			paperPos[(int)(p.x-minux)][(int)(p.y-minuy)][0]=p.px;
			paperPos[(int)(p.x-minux)][(int)(p.y-minuy)][1]=p.py;
			paperPos[(int)(p.x-minux)][(int)(p.y-minuy)][2]=p.pz;
			active[(int)(p.x-minux)][(int)(p.y-minuy)] = true;
		  }
		  else
		  {
			  printf("Bad coord: %f,%f\n",p.x,p.y);
		  }
		  
 		  numPoints++;

		}
		
		fclose(f);
      }
	  else
	    return false;
  }
	
  printf("Loaded %d points\n",numPoints);
	
  return true;
}

bool OutputSheet(char *filename, float minux, float minuy)
{
  FILE *f = fopen(filename,"w");

  gridPoint p;
  
  if (f)
  {	  
    for(int x = 0; x<SHEET_SIZE_X*DOWNSCALE; x++)
    for(int y = 0; y<SHEET_SIZE_Y*DOWNSCALE; y++)
    {
	  // output in x,y,z order 
	  if (activeOutput[x][y])
	  {
		  //p.x=x+minux*DOWNSCALE;p.y=y+minuy*DOWNSCALE;
		  p.x=x; p.y=y;
		  p.px=outputPaperPos[x][y][0];
		  p.py=outputPaperPos[x][y][1];
		  p.pz=outputPaperPos[x][y][2];
		  fwrite(&p,sizeof(p),1,f);
	  }
    }
    fclose(f);
    return true;
  }
  
  return false;
}

void Interpolate(void)
{
  float lower[3],upper[3];
  
  for(int x = 0; x<SHEET_SIZE_X*DOWNSCALE; x++)
  for(int y = 0; y<SHEET_SIZE_Y*DOWNSCALE; y++)
  {
	  int xs = x/DOWNSCALE;
	  int ys = y/DOWNSCALE;
	  int xm = x%DOWNSCALE;
	  int ym = y%DOWNSCALE;
	  
	  // deal with the case where all 4 corners are present
	  if (xs+1<SHEET_SIZE_X && ys+1<SHEET_SIZE_Y)
	  {
		if (active[xs][ys] && active[xs+1][ys] && active[xs][ys+1] && active[xs+1][ys+1])
		{
			// bilinear interpolation
			
			lower[0] = (paperPos[xs][ys][0] * (DOWNSCALE-xm) + paperPos[xs+1][ys][0] * xm)/DOWNSCALE;
			lower[1] = (paperPos[xs][ys][1] * (DOWNSCALE-xm) + paperPos[xs+1][ys][1] * xm)/DOWNSCALE;
			lower[2] = (paperPos[xs][ys][2] * (DOWNSCALE-xm) + paperPos[xs+1][ys][2] * xm)/DOWNSCALE;

			upper[0] = (paperPos[xs][ys+1][0] * (DOWNSCALE-xm) + paperPos[xs+1][ys+1][0] * xm)/DOWNSCALE;
			upper[1] = (paperPos[xs][ys+1][1] * (DOWNSCALE-xm) + paperPos[xs+1][ys+1][1] * xm)/DOWNSCALE;
			upper[2] = (paperPos[xs][ys+1][2] * (DOWNSCALE-xm) + paperPos[xs+1][ys+1][2] * xm)/DOWNSCALE;
			
			outputPaperPos[x][y][0] = (lower[0]*(DOWNSCALE-ym)+upper[0]*ym)/DOWNSCALE;
			outputPaperPos[x][y][1] = (lower[1]*(DOWNSCALE-ym)+upper[1]*ym)/DOWNSCALE;
			outputPaperPos[x][y][2] = (lower[2]*(DOWNSCALE-ym)+upper[2]*ym)/DOWNSCALE;
			
			activeOutput[x][y] = true;
		}
	  }
  }
}

int main(int argc, char *argv[])
{
	if (argc != 3 && argc != 5)
	{
		printf("Usage: interpolate <input> <output> [x-offset y-offset]\n");
		printf("Assumes that input gridPoints all have integer quadmesh coordinates - as output from simpaper\n");
	}
	
	int xoffset = 0, yoffset = 0;
	
	if (argc==5)
	{
		xoffset = atoi(argv[3]);
		yoffset = atoi(argv[4]);
	}
	
	float minux=0,minuy=0;
	
	if (LoadSheet(argv[1],xoffset,yoffset,minux,minuy))
	{
		Interpolate();
		if (!OutputSheet(argv[2],minux,minuy))
		{
		    fprintf(stderr,"Unable to open output file:%s\n",argv[2]);
			return -2;
		}
		
		return 0;
	}
	else
	{
		fprintf(stderr,"Unable to open input file:%s\n",argv[1]);
		return -1;
	}
}
