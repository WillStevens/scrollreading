#define VESUVIUS_IMPL
#include "../../vesuvius-c/vesuvius-c.h"
#include "../zarrlike/zarrlike.h"

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SIZE_X 256
#define SIZE_Y 256
#define SIZE_Z 256

#define SURFACE_VALUE 255

int vol_start[3] = {5120,2048,3200};
int chunk_dims[3] = {128,128,128};
int chunk_start[3] = {-1,-1,-1};

volume *scroll_vol;
chunk *scroll_chunk = NULL;

#define CHUNK_BUFFER_SIZE 8
int cache_starts[CHUNK_BUFFER_SIZE][3];
chunk *cache_chunks[CHUNK_BUFFER_SIZE];
uint64_t cache_used[CHUNK_BUFFER_SIZE];
uint64_t cache_counter;

chunk *get_cached_chunk(int start[3], bool debug)
{
	if (debug)
		printf("GCC A\n");
	for(int i = 0; i<CHUNK_BUFFER_SIZE; i++)
	{
		if (cache_chunks[i] && cache_starts[i][0] == start[0] && cache_starts[i][1] == start[1] && cache_starts[i][2] == start[2])
		{
			cache_used[i] = cache_counter++;
			if (debug)
				printf("GCC B\n");
			return cache_chunks[i];
		}
	}

	if (debug)
		printf("GCC C\n");
	
	for(int i = 0; i<CHUNK_BUFFER_SIZE; i++)
	{
		if (cache_chunks[i] == NULL)
		{
			if (debug)
	    	    printf("GCC D\n");

			int scroll_chunk_start[3] = {start[0]+vol_start[0],start[1]+vol_start[1],start[2]+vol_start[2]}; 
			cache_chunks[i] = vs_vol_get_chunk(scroll_vol, scroll_chunk_start,chunk_dims);
			cache_starts[i][0] = start[0];
			cache_starts[i][1] = start[1];
			cache_starts[i][2] = start[2];

			cache_used[i] = cache_counter++;
			if (debug)
				printf("GCC E\n");

			return cache_chunks[i];
		}
	}

	if (debug)
		printf("GCC F\n");

	uint64_t oldest = cache_used[0];
	int oldestIndex = 0;
	for(int i = 1; i<CHUNK_BUFFER_SIZE; i++)
	{
		if (cache_used[i] < oldest)
		{
			oldest = cache_used[i];
			oldestIndex = i;
		}
	}
	
	int i = oldestIndex;
	
	if (debug)
		printf("GCC G\n");
	
	// discard the oldest
	vs_chunk_free(cache_chunks[i]);
	int scroll_chunk_start[3] = {start[0]+vol_start[0],start[1]+vol_start[1],start[2]+vol_start[2]}; 
	cache_chunks[i] = vs_vol_get_chunk(scroll_vol, scroll_chunk_start,chunk_dims);
	if (cache_chunks[i] == NULL)
	{
		printf("ERROR - did not get chunk\n");
	}
	cache_starts[i][0] = start[0];
	cache_starts[i][1] = start[1];
	cache_starts[i][2] = start[2];

	cache_used[i] = cache_counter++;
			
	if (debug)
		printf("GCC H\n");

	return cache_chunks[i];	
}

// x,y,z are offset from vol_start
unsigned char get_scroll_point(int z, int y, int x, bool debug)
{
	if (z<chunk_start[0] || z>=chunk_start[0]+chunk_dims[0] ||
        y<chunk_start[1] || y>=chunk_start[1]+chunk_dims[1] ||
        x<chunk_start[2] || x>=chunk_start[2]+chunk_dims[2])
	{
		if (debug)
			printf("GSP A\n");
		scroll_chunk = NULL;
	}			
	if (!scroll_chunk)
	{
		if (debug)
			printf("GSP B\n");
		chunk_start[0]=z-z%chunk_dims[0];
		chunk_start[1]=y-y%chunk_dims[1];
		chunk_start[2]=x-x%chunk_dims[2];
		
		scroll_chunk = get_cached_chunk(chunk_start,debug);
	}
	if (debug)
		printf("GSP C\n");
	
	return vs_chunk_get(scroll_chunk,z%chunk_dims[0],y%chunk_dims[1],x%chunk_dims[2]);
}

int main() {
    blosc2_init();
	
    
    //initialize the volume
    /*
	volume* scroll_vol = vs_vol_new(
        "d:/zarrs/1213_aug_erode_threshold-ome.zarr/0/",
        "http://0.0.0.0:8080/1213_aug_erode_threshold-ome.zarr/0/");
    */

    	
	scroll_vol = vs_vol_new(
        "d:/zarrs/s1-surface-regular.zarr",
        "https://dl.ash2txt.org/community-uploads/bruniss/scrolls/s1/surfaces/full_scroll/s1-surface-erode.zarr/");
    
	
	
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
	ZARR *vfz = ZarrOpen("d:/pvf_508.zarr");
	ZARR *vfsz = ZarrOpen("d:/pvfs_508.zarr");
		
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
	for(int zc = 0; zc < SIZE_Z-VF_RAD; zc+=CHUNK_SIZE_Z)
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
		
            if (get_scroll_point(z,y,x,false) != SURFACE_VALUE)
			{
                int minCount = 0;
                float lastDist = 0;
                for(int i = 0; i<SORTED_DIST_SIZE; i++)
				{ 

                    if (get_scroll_point(z+sortedDistances[i][3],y+sortedDistances[i][2],x+sortedDistances[i][1],false) == SURFACE_VALUE)
					{

                        if (sortedDistances[i][0] != lastDist)
						{
                            if (minCount>0)
							{
                                vf[0] /= minCount;
                                vf[1] /= minCount;
                                vf[2] /= minCount;
								vf[3]=lastDist;


								if (firstInChunk)
								{
									ZarrWriteN(vfz,x,y,z,0,4,vf);
									firstInChunk = false;
								}
								else
								{
									ZarrNoCheckWriteN(vfz,x,y,z,0,4,vf);
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

    printf("Smoothing vector field...\n");

	float vf_smooth[4];
	int window = 1;
	for(int zc = 0; zc < SIZE_Z-VF_RAD; zc+=CHUNK_SIZE_Z)
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
			vf_smooth[0]=0.0;
			vf_smooth[1]=0.0;
			vf_smooth[2]=0.0;
			int tot = 0;
            for(int zo = (z-window<0?0:z-window); zo < (z+window+1<SIZE_Z-VF_RAD?z+window+1:SIZE_Z-VF_RAD); zo++)
            for(int yo = (y-window<0?0:y-window); yo < (y+window+1<SIZE_Y-VF_RAD?y+window+1:SIZE_Y-VF_RAD); yo++)
            for(int xo = (x-window<0?0:x-window); xo < (x+window+1<SIZE_X-VF_RAD?x+window+1:SIZE_X-VF_RAD); xo++)
			{
				ZarrReadN(vfz,xo,yo,zo,0,3,vf);
			    vf_smooth[0] += vf[0];
			    vf_smooth[1] += vf[1];
			    vf_smooth[2] += vf[2];
                tot ++;
			}
			
			vf_smooth[0] /= tot;
			vf_smooth[1] /= tot;
			vf_smooth[2] /= tot;
			// Get distance
            vf_smooth[3] = ZarrRead(vfz,x,y,z,3);

			if (firstInChunk)
			{
				ZarrWriteN(vfsz,x,y,z,0,4,vf_smooth);
				firstInChunk = false;
			}
			else
				ZarrNoCheckWriteN(vfsz,x,y,z,0,4,vf_smooth);
		}
	}
	}
	
	ZarrClose(vfz);
	ZarrClose(vfsz);
	
	blosc2_destroy();
    return 0;
}

