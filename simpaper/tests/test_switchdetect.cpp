#include <stdio.h>
#include <math.h>

/* Make a volume that contains two intersecting half-surfaces. Each of the surfaces is isometric to a plane, but when joined up they aren't.
   The first surface is part of a plane, the second is part of a curved plane */ 

#define SIZE 256   
#define ROUND(a) ((a-(int)a < 0.5)?(int)a : (int)a+1)
   
unsigned char volume[256][256][256];   
   
#define DILATE_CASE(xo,yo,zo,test) \
                if (test>=0 && test<SIZE) \
                { \
                    if (volume[z+zo][y+yo][x+xo]==255) \
                    { \
                        volume[z][y][x]=254; \
                        continue; \
                    } \
                }

void dilate(void)
{
    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
        if (volume[z][y][x]==0)
        {
			DILATE_CASE(0,0,1,z+1)
			DILATE_CASE(0,0,-1,z-1)
			DILATE_CASE(0,1,0,y+1)
			DILATE_CASE(0,-1,0,y-1)
			DILATE_CASE(1,0,0,x+1)
			DILATE_CASE(-1,0,0,x-1)
        }

    for(int z = 0; z<SIZE; z++)
    for(int y = 0; y<SIZE; y++)
    for(int x = 0; x<SIZE; x++)
        if (volume[z][y][x]==254)
            volume[z][y][x]=255;
}

int main(void)
{
  for(int z=0; z<256; z++)
  for(int y=0; y<256; y++)
  for(int x=0; x<256; x++)
    volume[z][y][x]=0;

  int r = 512;	
  for(int x=0; x<256; x++)
  for(int z=255; z>=0; z--)
  {
    /* We need (x-128)^2+(y-(128-r))^2 = r^2
               (y-(128-r))^2 = r^2 - (x-128)^2
			   (y-(128-r)) = sqrt(r^2 - (x-128)^2)
			   y = sqrt(r^2 - (x-128)^2) + (128-r)
    */ 
	
	float yf = sqrt(r*r - (x-128)*(x-128))+(128-r);
	int y = ROUND(yf);
	volume[z][y][x] = 1;
  }
  
  for(int x=0; x<256; x++)
  for(int z=0; z<256; z++)
  {
    float yf = 64+z/2;
	int y = ROUND(yf);
	
	bool found = (volume[z][y][x]==1);
	
    volume[z][y][x]=2;
	
	if (found)
      break;		
  }
  
  for(int x=0; x<256; x++)
  for(int z=255; z>=0; z--)
  {
    /* We need (x-128)^2+(y-(128-r))^2 = r^2
               (y-(128-r))^2 = r^2 - (x-128)^2
			   (y-(128-r)) = sqrt(r^2 - (x-128)^2)
			   y = sqrt(r^2 - (x-128)^2) + (128-r)
    */ 
	
	float yf = sqrt(r*r - (x-128)*(x-128))+(128-r);
	int y = ROUND(yf);

	bool found = (volume[z][y][x]==2);
	
	volume[z][y][x] = 2;

	if (found)
      break;		
  }

  for(int z=0; z<256; z++)
  for(int y=0; y<256; y++)
  for(int x=0; x<256; x++)
  {
	volume[z][y][x] = (volume[z][y][x]==2)?255:0;
  }

  dilate();
  dilate();
  
  for(int z=0; z<256; z++)
  for(int y=0; y<256; y++)
  for(int x=0; x<256; x++)
  {
	printf("%d\n",volume[z][y][x]);
  }
  
  return 0;
}