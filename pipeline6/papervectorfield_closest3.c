#include <math.h>
#include "zarr_1.c"
#include "zarr_c128i1b8.c"

#include "parameters.h"

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define CHUNK_SIZE_X 128
#define CHUNK_SIZE_Y 128
#define CHUNK_SIZE_Z 128

#define SURFACE_VALUE 255

int main(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Usage: papervectorfield_closest3 <surface zarr> <output zarr> <zmin> <zmax>\n");
		printf("Use zmin and zmax to have different threads produce different parts of the output zarr\n");
		exit(-1);
	}
	
	int zmin = atoi(argv[3]);
	int zmax = atoi(argv[4]);

	int ymin = 0;
	int ymax = VOL_SIZE_Y;
	
    blosc2_init();

    ZARR_1 *surfaceZarr = ZARROpen_1(argv[1]);
    
	printf("Creating ZARR\n");
	printf("Vector field is in the following order: x,y,z,distance\n");
	ZARR_c128i1b8 *vfz = ZARROpen_c128i1b8(argv[2]);
		
    // Precompute nearest voxels and associated distance and direction vector
    float sortedDistances[SORTED_DIST_SIZE][7];
	int i = 0;
    for(int zo = -VF_RAD; zo<=VF_RAD; zo++)
    for(int yo = -VF_RAD; yo<=VF_RAD; yo++)
    for(int xo = -VF_RAD; xo<=VF_RAD; xo++)
      if (xo!=0 || yo!=0 || zo != 0)
	  {
		float dist = sqrt(zo*zo+yo*yo+xo*xo);
        sortedDistances[i][0] = dist;
        sortedDistances[i][1] = xo;
        sortedDistances[i][2] = yo;
        sortedDistances[i][3] = zo;
        sortedDistances[i][4] = xo/dist;
        sortedDistances[i][5] = yo/dist;
        sortedDistances[i][6] = zo/dist;
		i++;
	  }

	// Bubble sort to get them in order 
	int done = 0;
	while(!done)
    {
		done = 1;
		for(int i = 0; i<SORTED_DIST_SIZE-1; i++)
		{
			if (sortedDistances[i][0]>sortedDistances[i+1][0])
			{
				for(int j = 0; j<7; j++)
				{
					float tmp = sortedDistances[i][j];
					sortedDistances[i][j] = sortedDistances[i+1][j];
					sortedDistances[i+1][j] = tmp;
				}
				done = 0;
			}
		}
	}

	for(int i = 0; i<SORTED_DIST_SIZE; i++)
	{
		printf("%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",sortedDistances[i][0],sortedDistances[i][1],sortedDistances[i][2],sortedDistances[i][3],sortedDistances[i][4],sortedDistances[i][5],sortedDistances[i][6]);
    }

    printf("Calculating vector field...\n");

	float vf[4];
	int8_t vfi[4];
	for(int zc = zmin; zc < zmax; zc+=CHUNK_SIZE_Z)
	for(int yc = ymin; yc < ymax; yc+=CHUNK_SIZE_Y)
	for(int xc = 0; xc < VOL_SIZE_X-VF_RAD; xc+=CHUNK_SIZE_X)
	{
	bool firstInChunk = true;
	printf("Z:%d,Y:%d,X:%d\n",zc,yc,xc);
    for(int z = zc<VF_RAD?VF_RAD:zc; z<zc+CHUNK_SIZE_Z && z<VOL_SIZE_Z-VF_RAD; z++)
	{
        printf("%d\n",z);
        for(int y = yc<VF_RAD?VF_RAD:yc; y<yc+CHUNK_SIZE_Y && y<VOL_SIZE_Y-VF_RAD; y++)
        for(int x = xc<VF_RAD?VF_RAD:xc; x<xc+CHUNK_SIZE_X && x<VOL_SIZE_X-VF_RAD; x++)
		{
			vf[0]=0.0;
			vf[1]=0.0;
			vf[2]=0.0;
		
		    uint8_t foundValue = ZARRRead_1(surfaceZarr,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X);
			
			{
                int minCount = 0;
                float lastDist = 0;
                for(int i = 0; i<SORTED_DIST_SIZE; i++)
				{ 

                    if (ZARRRead_1(surfaceZarr,z+sortedDistances[i][3]+VOL_OFFSET_Z,y+sortedDistances[i][2]+VOL_OFFSET_Y,x+sortedDistances[i][1]+VOL_OFFSET_X) != foundValue)
					{

                        if (sortedDistances[i][0] != lastDist)
						{
                            if (minCount>0)
							{
								/* If we're in a surface voxel, then point the vector away from the edge of the surface, but
								   if we're not in a surface voxel point it towards the surface */
                                vfi[0] = (int8_t)((vf[0] / minCount)*127) * (foundValue==SURFACE_VALUE?-1.0f:1.0f);
                                vfi[1] = (int8_t)((vf[1] / minCount)*127) * (foundValue==SURFACE_VALUE?-1.0f:1.0f);
                                vfi[2] = (int8_t)((vf[2] / minCount)*127) * (foundValue==SURFACE_VALUE?-1.0f:1.0f);
								/* we need vfi[3] to be zero in a surface and 255 around the boundaries of it */
								vfi[3] = 255-foundValue;


								if (firstInChunk)
								{
									ZARRWriteN_c128i1b8(vfz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,0,4,vfi);
									firstInChunk = false;
								}
								else
								{
									ZARRNoCheckWriteN_c128i1b8(vfz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,0,4,vfi);
								}

                                break;
							}
                            lastDist = sortedDistances[i][0];
						}
                        vf[0] += sortedDistances[i][4];
                        vf[1] += sortedDistances[i][5];
                        vf[2] += sortedDistances[i][6];
                        minCount += 1;
					}
				}
			
				if (i==SORTED_DIST_SIZE && minCount>0)
				{
					/* If we're in a surface voxel, then point the vector away from the edge of the surface, but
					if we're not in a surface voxel point it towards the surface */
					vfi[0] = (int8_t)((vf[0] / minCount)*127) * (foundValue==SURFACE_VALUE?-1.0f:1.0f);
					vfi[1] = (int8_t)((vf[1] / minCount)*127) * (foundValue==SURFACE_VALUE?-1.0f:1.0f);
					vfi[2] = (int8_t)((vf[2] / minCount)*127) * (foundValue==SURFACE_VALUE?-1.0f:1.0f);
					/* we need vfi[3] to be zero in a surface and 255 around the boundaries of it */
					vfi[3] = 255-foundValue;


					if (firstInChunk)
					{
						ZARRWriteN_c128i1b8(vfz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,0,4,vfi);
						firstInChunk = false;
					}
					else
					{
						ZARRNoCheckWriteN_c128i1b8(vfz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,0,4,vfi);
					}
				}
				
			}
		}
	}}

	ZARRClose_1(surfaceZarr);

	printf("Finished, closing ZARRs\n");
	
	ZARRClose_c128i1b8(vfz);
	
	printf("Calling blosc2_destroy\n");
	
	blosc2_destroy();
    return 0;
}

