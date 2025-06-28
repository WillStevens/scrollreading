#include <stdio.h>

#define SHEET_SIZE 1000
#define STEP_SIZE 3

float paperPos[SHEET_SIZE][SHEET_SIZE][3];
bool active[SHEET_SIZE][SHEET_SIZE];

float outputPaperPos[SHEET_SIZE*STEP_SIZE][SHEET_SIZE*STEP_SIZE][3];
bool activeOutput[SHEET_SIZE*STEP_SIZE][SHEET_SIZE*STEP_SIZE];

void LoadSheet(char *filename)
{
  FILE *f = fopen(filename,"r");

  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
  {
	active[x][y] = false; 
  }

  int x,y;
  float px,py,pz;
  
  // input in x,y,z order 
  while(fscanf(f,"%d,%d,%f,%f,%f\n",&x,&y,&px,&py,&pz)==5)
  {
    paperPos[x][y][0]=px;
	paperPos[x][y][1]=py;
	paperPos[x][y][2]=pz;
    active[x][y] = true;	
  }
  fclose(f);
}

void OutputSheet(char *filename)
{
  FILE *f = fopen(filename,"w");
  
  for(int x = 0; x<SHEET_SIZE*STEP_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE*STEP_SIZE; y++)
  {
	// output in x,y,z order 
	if (activeOutput[x][y])
      fprintf(f,"%d,%d,%f,%f,%f\n",x,y,outputPaperPos[x][y][0],outputPaperPos[x][y][1],outputPaperPos[x][y][2]); 
  }
  fclose(f);
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
			
			lower[0] = (paperPos[xs][ys][0] * (3-xm) + paperPos[xs+1][ys][0] * xm)/3.0;
			lower[1] = (paperPos[xs][ys][1] * (3-xm) + paperPos[xs+1][ys][1] * xm)/3.0;
			lower[2] = (paperPos[xs][ys][2] * (3-xm) + paperPos[xs+1][ys][2] * xm)/3.0;

			upper[0] = (paperPos[xs][ys+1][0] * (3-xm) + paperPos[xs+1][ys+1][0] * xm)/3.0;
			upper[1] = (paperPos[xs][ys+1][1] * (3-xm) + paperPos[xs+1][ys+1][1] * xm)/3.0;
			upper[2] = (paperPos[xs][ys+1][2] * (3-xm) + paperPos[xs+1][ys+1][2] * xm)/3.0;
			
			outputPaperPos[x][y][0] = (lower[0]*(3-ym)+upper[0]*ym)/3.0;
			outputPaperPos[x][y][1] = (lower[1]*(3-ym)+upper[1]*ym)/3.0;
			outputPaperPos[x][y][2] = (lower[2]*(3-ym)+upper[2]*ym)/3.0;
			
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
	}
	
	LoadSheet(argv[1]);
    Interpolate();
	OutputSheet(argv[2]);
}
