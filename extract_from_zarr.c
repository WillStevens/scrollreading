#define VESUVIUS_IMPL
#include "../vesuvius-c/vesuvius-c.h"

#define SIZE_X 512
#define SIZE_Y 512
#define SIZE_Z 512

int main() {

    int vol_start[3] = {2432-256,2304,4096};
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
	
	for(int z=0; z<SIZE_Z; z+=16)
	{
		slice* myslice = vs_slice_extract(scroll_chunk, z);

		char fname[100];
		
		sprintf(fname,"tmp\\papervectorfield_test_surface_%05d.bmp",z);
        // Write slice image to file
        vs_bmp_write(fname,myslice);
    }
	
    printf("Extracting from zarr...\n");

	FILE *f = fopen("d:/zarr_tmp_extract.csv","w");
	
    for(int z = 0; z<SIZE_Z; z++)
	{
        printf("%d\n",z);
        for(int y = 0; y<SIZE_Y; y++)
        for(int x = 0; x<SIZE_X; x++)
		{
            int v = vs_chunk_get(scroll_chunk,z,y,x);
			fprintf(f,"%d\n",v);
		}
	}
	
	fclose(f);
	
    return 0;
}
