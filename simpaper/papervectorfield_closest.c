#define VESUVIUS_IMPL
#include "../vesuvius-c/vesuvius-c.h"

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SIZE_X 256
#define SIZE_Y 256
#define SIZE_Z 256

#define SURFACE_VALUE 255

float ****MakeFloatVectorArray(int x, int y, int z)
{
	float ****zarray = malloc(z*sizeof(float***));
	
	for(int zi=0; zi<z; zi++)
	{
      printf("%d\n",zi);
	  zarray[zi] = malloc(y*sizeof(float**));
	  for(int yi=0; yi<y; yi++)
	  {
		  zarray[zi][yi] = malloc(x*sizeof(float*));
		  for(int xi=0; xi<x; xi++)
		  {
			  zarray[zi][yi][xi] = malloc(3*sizeof(float));
			  zarray[zi][yi][xi][0]=0.0f;
			  zarray[zi][yi][xi][1]=0.0f;
			  zarray[zi][yi][xi][2]=0.0f;
		  }
	  }
	}
	
	return zarray;
}

float ***MakeFloatArray(int x, int y, int z)
{
	float ***zarray = malloc(z*sizeof(float**));
	
	for(int zi=0; zi<z; zi++)
	{
	  zarray[zi] = malloc(y*sizeof(float*));
	  for(int yi=0; yi<y; yi++)
	  {
		  zarray[zi][yi] = malloc(x*sizeof(float));
		  for(int xi=0; xi<x; xi++)
		  {
			  zarray[zi][yi][xi] = 0.0f;
		  }
	  }
	}
	
	return zarray;
}



int main() {
/*
    //pick a region in the scoll to visualize
    int vol_start[3] = {3072,3072,3072};
    int chunk_dims[3] = {128,512,512};
    
    //initialize the volume
    volume* scroll_vol = vs_vol_new(
        "./54keV_7.91um_Scroll1A.zarr/0/",
        "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/");
    
    // get the scroll data by reading it from the cache and downloading it if necessary
    chunk* scroll_chunk = vs_vol_get_chunk(scroll_vol, vol_start,chunk_dims);

    // Fetch a slice  from the volume
    slice* myslice = vs_slice_extract(scroll_chunk, 0);

    // Write slice image to file
    vs_bmp_write("xy_slice.bmp",myslice);
*/

    //pick a region in the scoll to visualize
//    int vol_start[3] = {2432,2304,4096};
    int vol_start[3] = {2432,2304,4096};
    int chunk_dims[3] = {SIZE_Z,SIZE_Y,SIZE_X};
    
    //initialize the volume
    /*
	volume* scroll_vol = vs_vol_new(
        "d:/zarrs/1213_aug_erode_threshold-ome.zarr/0/",
        "http://0.0.0.0:8080/1213_aug_erode_threshold-ome.zarr/0/");
    */

    	
	volume* scroll_vol = vs_vol_new(
        "d:/zarrs/s1-surface-regular.zarr",
        "https://dl.ash2txt.org/community-uploads/bruniss/scrolls/s1/surfaces/full_scroll/s1-surface-erode.zarr/");
    

	/*
	volume* scroll_vol = vs_vol_new(
        "d:/zarrs/s1a-rectoverso-06032025-ome.zarr/0/",
        "https://dl.ash2txt.org/other/dev/meshes/s1a-rectoverso-06032025-ome.zarr/0/");
	*/
	
    // get the scroll data by reading it from the cache and downloading it if necessary
    chunk* scroll_chunk = vs_vol_get_chunk(scroll_vol, vol_start,chunk_dims);

	
	for(int z=0; z<SIZE_Z; z++)
	{
		slice* myslice = vs_slice_extract(scroll_chunk, z);

		char fname[100];
		
		sprintf(fname,"tmp\\papervectorfield_test_surface_%05d.bmp",z);
        // Write slice image to file
        vs_bmp_write(fname,myslice);
    }
	
	int vf_dims[3];
	
	for(int i = 0; i<3; i++)
	  vf_dims[i] = chunk_dims[i]-2*VF_RAD;

    /*
    static float vf[SIZE_Z-2*VF_RAD][SIZE_Y-2*VF_RAD][SIZE_X-2*VF_RAD][3];
    static float vf_smooth[SIZE_Z-2*VF_RAD][SIZE_Y-2*VF_RAD][SIZE_X-2*VF_RAD][3];
	static float distance[SIZE_Z-2*VF_RAD][SIZE_Y-2*VF_RAD][SIZE_X-2*VF_RAD];
	*/
	printf("Allocating vf\n");
	float ****vf = MakeFloatVectorArray(SIZE_Z-2*VF_RAD,SIZE_Y-2*VF_RAD,SIZE_X-2*VF_RAD);
	printf("Allocating distance\n");
	float ***distance = MakeFloatArray(SIZE_Z-2*VF_RAD,SIZE_Y-2*VF_RAD,SIZE_X-2*VF_RAD);
	
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

    for(int z = 0; z<vf_dims[0]; z++)
	{
        printf("%d\n",z);
        for(int y = 0; y<vf_dims[1]; y++)
        for(int x = 0; x<vf_dims[2]; x++)
		{
			vf[z][y][x][0]=0.0;
			vf[z][y][x][1]=0.0;
			vf[z][y][x][2]=0.0;
			distance[z][y][x]=0.0;
			
            if (vs_chunk_get(scroll_chunk,z+VF_RAD,y+VF_RAD,x+VF_RAD) != SURFACE_VALUE)
			{
                int minCount = 0;
                float lastDist = 0;
                for(int i = 0; i<SORTED_DIST_SIZE; i++)
				{
                    if (vs_chunk_get(scroll_chunk,z+VF_RAD+sortedDistances[i][3],y+VF_RAD+sortedDistances[i][2],x+VF_RAD+sortedDistances[i][1]) == SURFACE_VALUE)
					{
                        if (sortedDistances[i][0] != lastDist)
						{
                            if (minCount>0)
							{
                                vf[z][y][x][0] /= minCount;
                                vf[z][y][x][1] /= minCount;
                                vf[z][y][x][2] /= minCount;
								distance[z][y][x]=lastDist;

                                break;
							}
                            lastDist = sortedDistances[i][0];
						}
                        vf[z][y][x][0] += sortedDistances[i][4];
                        vf[z][y][x][1] += sortedDistances[i][5];
                        vf[z][y][x][2] += sortedDistances[i][6];
                        minCount += 1;
					}
				}
			}
		}
	}

    printf("Smoothing vector field...\n");

	float vf_smooth[3];
	
	FILE *f = fopen("vectorfield_test_smooth_508.csv","w");

	int window = 1;
    for(int z = 0; z<vf_dims[0]; z++)
	{
        printf("%d\n",z);
        for(int y = 0; y<vf_dims[1]; y++)
        for(int x = 0; x<vf_dims[2]; x++)
		{
			vf_smooth[0]=0.0;
			vf_smooth[1]=0.0;
			vf_smooth[2]=0.0;
			int tot = 0;
            for(int zo = (z-window<0?0:z-window); zo < (z+window+1<vf_dims[0]?z+window+1:vf_dims[0]); zo++)
            for(int yo = (y-window<0?0:y-window); yo < (y+window+1<vf_dims[1]?y+window+1:vf_dims[1]); yo++)
            for(int xo = (x-window<0?0:x-window); xo < (x+window+1<vf_dims[2]?x+window+1:vf_dims[2]); xo++)
			{
			    vf_smooth[0] += vf[zo][yo][xo][0];
			    vf_smooth[1] += vf[zo][yo][xo][1];
			    vf_smooth[2] += vf[zo][yo][xo][2];
                tot ++;
			}
			
			vf_smooth[0] /= tot;
			vf_smooth[1] /= tot;
			vf_smooth[2] /= tot;

			fprintf(f,"%f,%f,%f,%f\n",vf_smooth[0],vf_smooth[1],vf_smooth[2],distance[z][y][x]);
		}
	}
	
	fclose(f);
	
    return 0;
}

/*
      
for z in range(vfRad,size-vfRad-1):
  print(z)
  for y in range(vfRad,size-vfRad-1):
    for x in range(vfRad,size-vfRad-1):
      if k[z,y,x] != 255:
        minCount = 0
        lastDist = None
        for (dist,xo,yo,zo,v) in sortedDistances:
          if k[z+zo,y+yo,x+xo] == 255:
            if dist != lastDist:
              if minCount>0:
                vf[z-vfRad,y-vfRad,x-vfRad,:] /= minCount
                break
              lastDist = dist
            vf[z-vfRad,y-vfRad,x-vfRad,:] += v
            minCount += 1
      
print("Outputting...")
for i in range(0,vfSize):
  j = Image.fromarray(vf[i,:,:,0]/10.0+0.5)
  j.save("tmp\\papervectorfield_test_x_%03d.tif"%i)
  j = Image.fromarray(vf[i,:,:,1]/10.0+0.5)
  j.save("tmp\\papervectorfield_test_y_%03d.tif"%i)
  j = Image.fromarray(vf[i,:,:,2]/10.0+0.5)
  j.save("tmp\\papervectorfield_test_z_%03d.tif"%i)

np.save("vectorfield_test_100.npy",vf)
*/
