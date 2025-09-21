#define VESUVIUS_IMPL
#include "../../../vesuvius-c/vesuvius-c.h"

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SIZE_X 512
#define SIZE_Y 512
#define SIZE_Z 512

#define SURFACE_VALUE 255
// TODO about to do the second one of these
int vol_start[3] = {5120-512,2048-512,3200-512};
int chunk_dims[3] = {512,512,512};
int chunk_start[3] = {-1,-1,-1};

volume *scroll_vol;
chunk *scroll_chunk = NULL;

int main(int argc, char *argv[]) {
	if (argc != 4)
	{
		printf("Usage: buffer_zar <z> <y> <x>\n");
		printf("Buffers 512 x 512 x 512 volumes onto disk\n");
		exit(-1);
	}
	
	vol_start[0] = atoi(argv[1]);
	vol_start[1] = atoi(argv[2]);
	vol_start[2] = atoi(argv[3]);
	
	printf("%d,%d,%d\n",vol_start[0],vol_start[1],vol_start[2]);
	
    blosc2_init();
    	
		/*
	scroll_vol = vs_vol_new(
        "d:/zarrs/s1-surface-erode.zarr",
        "https://dl.ash2txt.org/community-uploads/bruniss/scrolls/s1/surfaces/full_scroll/s1-surface-erode.zarr/");
    */

	/* used for d:/pvfs_2048_chunk_32.zarr */
	
	scroll_vol = vs_vol_new(
        "d:/zarrs/s1_059_ome.zarr",
        "https://dl.ash2txt.org/community-uploads/bruniss/scrolls/s1/surfaces/s1_059_ome.zarr/0/");
	
	/*
    volume* scroll_vol = vs_vol_new(
        "d:/zarrs/54keV_7.91um_Scroll1A.zarr/0/",
        "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volumes_zarr_standardized/54keV_7.91um_Scroll1A.zarr/0/");
	*/
	
	
    // get the scroll data by reading it from the cache and downloading it if necessary
    scroll_chunk = vs_vol_get_chunk(scroll_vol, vol_start,chunk_dims);

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
	
	blosc2_destroy();
    return 0;
}
