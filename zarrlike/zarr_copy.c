/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/


#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "zarr_3_16b.c"
#include "zarr_5.c"

ZARR_3 *za_src; // 128x128x128x4 chunks
ZARR_5 *za_dst; // 32x32x32x4 chunks

int main(int argc, char *argv[])
{		
    if (argc != 3)
	{
		printf("Usage: zarr_copy zmin zmax\n");
		exit(0);
	}
	
	int zmin = atoi(argv[1]);
	int zmax = atoi(argv[2]);
	
	za_src = ZARROpen_3("d:/pvfs_2048.zarr");
	za_dst = ZARROpen_5("d:/pvfs_2048_chunk_32.zarr");
	
	float vf[4*32];
	
	for(int z=zmin; z<zmax; z+=128)
	for(int y=0; y<2048; y+=128)
	for(int x=0; x<2048; x+=128)
	{
		printf("Chunk: %d,%d,%d\n",z,y,x);
		for(int zp=z; zp<z+128; zp+=32)
		for(int yp=y; yp<y+128; yp+=32)
		for(int xp=x; xp<x+128; xp+=32)
		{
		  for(int zo=zp; zo<zp+32; zo++)
		  for(int yo=yp; yo<yp+32; yo++)
		  {
			ZARRReadN_3(za_src,zo,yo,xp,0,4*32,vf);
			ZARRWriteN_5(za_dst,zo,yo,xp,0,4*32,vf);
		  }
		}
	}
	
	ZARRClose_5(za_dst);
	ZARRClose_3(za_src);
	
	return 0;
}

