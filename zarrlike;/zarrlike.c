#include "zarrlike.h"

int main(void)
{
  int index[4];
  
  ZARR *zarr = ZarrOpen("d:/toy.zarr");

  for(int cz = 0; cz<2; cz++)
  for(int cy = 0; cy<2; cy++)
  for(int cx = 0; cx<2; cx++)
  for(int z = 0; z<CHUNK_SIZE_Z; z++)
  for(int y = 0; y<CHUNK_SIZE_Y; y++)
  for(int x = 0; x<CHUNK_SIZE_X; x++)
  for(int w = 0; w<CHUNK_SIZE_W; w++)
  {
    index[0]=z+cz*CHUNK_SIZE_Z; index[1]=y+cy*CHUNK_SIZE_Y; index[2]=x+cx*CHUNK_SIZE_X; index[3]=w;
    float tval = (z+2.1*y+3.2*x+w*5.32)+(cz*1.324+cy*94.31+cx*0.23);
    ZarrWrite(zarr,index,tval);
  }
  
  printf("Here\n");
  int count = 0;
  for(int cz = 0; cz<2; cz++)
  for(int cy = 0; cy<2; cy++)
  for(int cx = 0; cx<2; cx++)
  for(int z = 0; z<CHUNK_SIZE_Z; z++)
  for(int y = 0; y<CHUNK_SIZE_Y; y++)
  for(int x = 0; x<CHUNK_SIZE_X; x++)
  for(int w = 0; w<CHUNK_SIZE_W; w++)
  {
    index[0]=z+cz*CHUNK_SIZE_Z; index[1]=y+cy*CHUNK_SIZE_Y; index[2]=x+cx*CHUNK_SIZE_X; index[3]=w;
    float v = ZarrRead(zarr,index);

    float tval = (z+2.1*y+3.2*x+w*5.32)+(cz*1.324+cy*94.31+cx*0.23);
	
	if (v != tval)
	{
		printf("Mismatch:%f %f\n",v,tval);
	}
	count++;
	if (count%1000000==0)
		printf("%d\n",count);
  }
  
  ZarrClose(zarr);  
}