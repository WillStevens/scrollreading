/* This is deliberately not done using separable smoothing because otherwise the zarr would have to be read/written 3 times
 * and the operation is I/O bound. Only for large GAUSS_WINDOW would it be more efficient to use separable smoothing */
#include <math.h>
#include "../../zarrlike/zarr_c128i1b16.c"
#include "../../zarrlike/zarr_c32i1b1.c"

#define VF_RAD 2

#define SIZE_X 2048
#define SIZE_Y 2560
#define SIZE_Z 4096

#define CHUNK_SIZE_X 32
#define CHUNK_SIZE_Y 32
#define CHUNK_SIZE_Z 32

#define GAUSS_WINDOW 3
#define GAUSS_SCALE 1

#include "gaussian_kernel_3d.c"

int vol_start[3] = {4608,1536,2688};

int main(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Usage: vectorfield_smooth_32 <source zarr> <dest zarr> <zmin> <zmax>\n");
		printf("Use zmin and zmax to have different threads produce different parts of the output zarr\n");
		
		exit(-1);
	}
	
	int zmin = atoi(argv[3]);
	int zmax = atoi(argv[4]);
	
	int ymin = 1920;
	int ymax = 2560;
	
    blosc2_init();

	printf("Opening ZARRs\n");
	
    ZARR_c128i1b16 *vfz = ZARROpen_c128i1b16(argv[1]);
    	
	ZARR_c32i1b1 *vfsz = ZARROpen_c32i1b1(argv[2]); // 32x32x32x4

	
    printf("Smoothing vector field...\n");
	
	int8_t vf[3];
	float vf_smooth[4];
	int8_t vfi_smooth[4];
    float min=0,max=0;
	
	for(int zc = zmin; zc < zmax; zc+=CHUNK_SIZE_Z)
	for(int yc = ymin; yc < ymax; yc+=CHUNK_SIZE_Y)
	for(int xc = 0; xc < SIZE_X-VF_RAD; xc+=CHUNK_SIZE_X)
	{
		bool firstInChunk = true;
		printf("Z:%d,Y:%d,X:%d\n",zc,yc,xc);		
	    printf("min=%f\n",min);
	    printf("max=%f\n",max);
		for(int z = zc; z<zc+CHUNK_SIZE_Z; z++)
		for(int y = yc; y<yc+CHUNK_SIZE_Y; y++)
		for(int x = xc; x<xc+CHUNK_SIZE_X; x++)
		{
			vf_smooth[0]=0.0;
			vf_smooth[1]=0.0;
			vf_smooth[2]=0.0;
			
			for(int zo = z-GAUSS_WINDOW; zo<=z+GAUSS_WINDOW; zo++)
			{
  			  int zod=zo;
		      if (zo<0) zod = -zo;
			  if (zo>SIZE_Z-1) zod = 2*SIZE_Z-zo-1;
			  for(int yo = y-GAUSS_WINDOW; yo<=y+GAUSS_WINDOW; yo++)
			  {
			    int yod=yo;
		        if (yo<0) yod = -yo;
			    if (yo>SIZE_Y-1) yod = 2*SIZE_Y-yo-1;
			    for(int xo = x-GAUSS_WINDOW; xo<=x+GAUSS_WINDOW; xo++)
			    {
				  int xod=xo;
				
			      if (xo<0) xod = -xo;
				  if (xo>SIZE_X-1) xod = 2*SIZE_X-xo-1;
				
				  ZARRReadN_c128i1b16(vfz,zod+vol_start[0],yod+vol_start[1],xod+vol_start[2],0,3,vf);
				
                  //printf("%d %d %d\n",(int)vf[0],(int)vf[1],(int)vf[2]);				
				  vf_smooth[0] += kernel[zod-z+GAUSS_WINDOW][yod-y+GAUSS_WINDOW][xod-x+GAUSS_WINDOW]*vf[0];
				  vf_smooth[1] += kernel[zod-z+GAUSS_WINDOW][yod-y+GAUSS_WINDOW][xod-x+GAUSS_WINDOW]*vf[1];
				  vf_smooth[2] += kernel[zod-z+GAUSS_WINDOW][yod-y+GAUSS_WINDOW][xod-x+GAUSS_WINDOW]*vf[2];	  
			    }
			  }
			}
				
			for(int i = 0; i<3; i++)
			{
				if (vf_smooth[i]>max) max = vf_smooth[i];
				if (vf_smooth[i]<min) min = vf_smooth[i];
			}
			
			vfi_smooth[0] = vf_smooth[0]*GAUSS_SCALE;
			vfi_smooth[1] = vf_smooth[1]*GAUSS_SCALE;
			vfi_smooth[2] = vf_smooth[2]*GAUSS_SCALE;
				
			vfi_smooth[3] = ZARRRead_c128i1b16(vfz,z+vol_start[0],y+vol_start[1],x+vol_start[2],3);
					
			if (firstInChunk)
			{
				ZARRWriteN_c32i1b1(vfsz,z+vol_start[0],y+vol_start[1],x+vol_start[2],0,4,vfi_smooth);
				firstInChunk = false;
			}
			else
			{
				ZARRNoCheckWriteN_c32i1b1(vfsz,z+vol_start[0],y+vol_start[1],x+vol_start[2],0,4,vfi_smooth);
			}
        }			
	}

	printf("Finished, closing ZARRs\n");
	
	ZARRClose_c128i1b16(vfz);
	ZARRClose_c32i1b1(vfsz);
	
	printf("Calling blosc2_destroy\n");
	
	blosc2_destroy();
	
	printf("min=%f\n",min);
	printf("max=%f\n",max);
    return 0;
}

