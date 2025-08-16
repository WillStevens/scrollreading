#include <stdio.h>

#define SHEET_SIZE 1000
#define STEP_SIZE 6

float paperPos[SHEET_SIZE][SHEET_SIZE][3];
bool active[SHEET_SIZE][SHEET_SIZE];

float outputPaperPos[SHEET_SIZE*STEP_SIZE][SHEET_SIZE*STEP_SIZE][3];
bool activeOutput[SHEET_SIZE*STEP_SIZE][SHEET_SIZE*STEP_SIZE];

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPoint;

bool LoadSheet(char *filename)
{
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
  {
	active[x][y] = false; 
  }
  
  gridPoint p;
  
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
	
      paperPos[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2][0]=p.px;
	  paperPos[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2][1]=p.py;
	  paperPos[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2][2]=p.pz;
      active[(int)p.x+SHEET_SIZE/2][(int)p.y+SHEET_SIZE/2] = true;
    }
	
    fclose(f);
	
	return true;
  }
  
  return false;
}

bool OutputSheet(char *filename)
{
  FILE *f = fopen(filename,"w");

  gridPoint p;
  
  if (f)
  {	  
    for(int x = 0; x<SHEET_SIZE*STEP_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE*STEP_SIZE; y++)
    {
	  // output in x,y,z order 
	  if (activeOutput[x][y])
	  {
		  p.x=x-(SHEET_SIZE*STEP_SIZE)/2;p.y=y-(SHEET_SIZE*STEP_SIZE)/2;
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
  
  for(int x = 0; x<SHEET_SIZE*STEP_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE*STEP_SIZE; y++)
  {
	  int xs = x/STEP_SIZE;
	  int ys = y/STEP_SIZE;
	  int xm = x%STEP_SIZE;
	  int ym = y%STEP_SIZE;
	  
	  // deal with the case where all 4 corners are present
	  if (xs+1<SHEET_SIZE && ys+1<SHEET_SIZE)
	  {
		if (active[xs][ys] && active[xs+1][ys] && active[xs][ys+1] && active[xs+1][ys+1])
		{
			// bilinear interpolation
			
			lower[0] = (paperPos[xs][ys][0] * (STEP_SIZE-xm) + paperPos[xs+1][ys][0] * xm)/STEP_SIZE;
			lower[1] = (paperPos[xs][ys][1] * (STEP_SIZE-xm) + paperPos[xs+1][ys][1] * xm)/STEP_SIZE;
			lower[2] = (paperPos[xs][ys][2] * (STEP_SIZE-xm) + paperPos[xs+1][ys][2] * xm)/STEP_SIZE;

			upper[0] = (paperPos[xs][ys+1][0] * (STEP_SIZE-xm) + paperPos[xs+1][ys+1][0] * xm)/STEP_SIZE;
			upper[1] = (paperPos[xs][ys+1][1] * (STEP_SIZE-xm) + paperPos[xs+1][ys+1][1] * xm)/STEP_SIZE;
			upper[2] = (paperPos[xs][ys+1][2] * (STEP_SIZE-xm) + paperPos[xs+1][ys+1][2] * xm)/STEP_SIZE;
			
			outputPaperPos[x][y][0] = (lower[0]*(STEP_SIZE-ym)+upper[0]*ym)/STEP_SIZE;
			outputPaperPos[x][y][1] = (lower[1]*(STEP_SIZE-ym)+upper[1]*ym)/STEP_SIZE;
			outputPaperPos[x][y][2] = (lower[2]*(STEP_SIZE-ym)+upper[2]*ym)/STEP_SIZE;
			
			activeOutput[x][y] = true;
		}
	  }
  }
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: interpolate <input> <output>\n");
		printf("Assumes that input gridPoints all have integer quadmesh coordinates - as output from simpaper\n");
	}
	
	if (LoadSheet(argv[1]))
	{
		Interpolate();
		if (!OutputSheet(argv[2]))
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
