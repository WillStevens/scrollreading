#include <math.h>
#include "../../zarrlike/zarr_1.c"
#include "../../zarrlike/zarr_c128i1b8.c"

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SIZE_X 2048
#define SIZE_Y 2048
#define SIZE_Z 4096

#define CHUNK_SIZE_X 128
#define CHUNK_SIZE_Y 128
#define CHUNK_SIZE_Z 128

#define SURFACE_VALUE 255

int vol_start[3] = {4608,1536,2688};
int chunk_dims[3] = {128,128,128};
int chunk_start[3] = {-1,-1,-1};


int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: papervectorfield_closest <zmin> <zmax>\n");
		printf("Use zmin and zmax to have different threads produce different parts of the output zarr\n");
		exit(-1);
	}
	
	int zmin = atoi(argv[1]);
	int zmax = atoi(argv[2]);
	
    blosc2_init();
	
    
    //initialize the volume
    /*
	volume* scroll_vol = vs_vol_new(
        "d:/zarrs/1213_aug_erode_threshold-ome.zarr/0/",
        "http://0.0.0.0:8080/1213_aug_erode_threshold-ome.zarr/0/");
    */


    ZARR_1 *surfaceZarr = ZARROpen_1("D:/zarrs/s1_059_ome.zarr");
    
	
	
	/*
	volume* scroll_vol = vs_vol_new(
        "d:/zarrs/s1a-rectoverso-06032025-ome.zarr/0/",
        "https://dl.ash2txt.org/other/dev/meshes/s1a-rectoverso-06032025-ome.zarr/0/");
	*/
	
    // get the scroll data by reading it from the cache and downloading it if necessary
    //scroll_chunk = vs_vol_get_chunk(scroll_vol, vol_start,chunk_dims);

	/*
	for(int z=0; z<SIZE_Z; z++)
	{
		slice* myslice = vs_slice_extract(scroll_chunk, z);

		char fname[100];
		
		sprintf(fname,"tmp\\papervectorfield_test_surface_%05d.bmp",z);
        // Write slice image to file
        vs_bmp_write(fname,myslice);
    }
	*/

    /*
    static float vf[SIZE_Z-2*VF_RAD][SIZE_Y-2*VF_RAD][SIZE_X-2*VF_RAD][3];
    static float vf_smooth[SIZE_Z-2*VF_RAD][SIZE_Y-2*VF_RAD][SIZE_X-2*VF_RAD][3];
	static float distance[SIZE_Z-2*VF_RAD][SIZE_Y-2*VF_RAD][SIZE_X-2*VF_RAD];
	*/
	printf("Creating ZARR\n");
	printf("Vector field is in the following order: x,y,z,distance\n");
//	ZARR_c128i1b8 *vfz = ZARROpen_c128i1b8("d:/pvf_2048_i1.zarr");
	ZARR_c128i1b8 *vfz = ZARROpen_c128i1b8("d:/temp");
//	ZARR *vfsz = ZarrOpen("d:/pvfs_508.zarr");
		
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
	for(int yc = 0; yc < SIZE_Y-VF_RAD; yc+=CHUNK_SIZE_Y)
	for(int xc = 0; xc < SIZE_X-VF_RAD; xc+=CHUNK_SIZE_X)
	{
	bool firstInChunk = true;
	printf("Z:%d,Y:%d,X:%d\n",zc,yc,xc);
    for(int z = zc<VF_RAD?VF_RAD:zc; z<zc+CHUNK_SIZE_Z && z<SIZE_Z-VF_RAD; z++)
	{
        printf("%d\n",z);
        for(int y = yc<VF_RAD?VF_RAD:yc; y<yc+CHUNK_SIZE_Y && y<SIZE_Y-VF_RAD; y++)
        for(int x = xc<VF_RAD?VF_RAD:xc; x<xc+CHUNK_SIZE_X && x<SIZE_X-VF_RAD; x++)
		{
			vf[0]=0.0;
			vf[1]=0.0;
			vf[2]=0.0;
			vf[3]=100.0; // distance
		
            if (ZARRRead_1(surfaceZarr,z+vol_start[0],y+vol_start[1],x+vol_start[2]) != SURFACE_VALUE)
			{
                int minCount = 0;
                float lastDist = 0;
                for(int i = 0; i<SORTED_DIST_SIZE; i++)
				{ 

                    if (ZARRRead_1(surfaceZarr,z+sortedDistances[i][3]+vol_start[0],y+sortedDistances[i][2]+vol_start[1],x+sortedDistances[i][1]+vol_start[2]) == SURFACE_VALUE)
					{

                        if (sortedDistances[i][0] != lastDist)
						{
                            if (minCount>0)
							{
                                vfi[0] = (int8_t)((vf[0] / minCount)*127);
                                vfi[1] = (int8_t)((vf[1] / minCount)*127);
                                vfi[2] = (int8_t)((vf[2] / minCount)*127);
								vfi[3]=(int8_t)lastDist;


								if (firstInChunk)
								{
									ZARRWriteN_c128i1b8(vfz,z+vol_start[0],y+vol_start[1],x+vol_start[2],0,4,vfi);
									firstInChunk = false;
								}
								else
								{
									ZARRNoCheckWriteN_c128i1b8(vfz,z+vol_start[0],y+vol_start[1],x+vol_start[2],0,4,vfi);
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

