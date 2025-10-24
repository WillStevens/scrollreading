/* This is deliberately not done using separable smoothing because otherwise the zarr would have to be read/written 3 times
 * and the operation is I/O bound. Only for large SMOOTH_WINDOW would it be more efficient to use separable smoothing */
#include <math.h>
#include "zarr_c128i1b16.c"
#include "zarr_c64i1b1.c"

#include "parameters.h"

#define SMOOTH_WINDOW 2
#define GAUSS_SCALE 1

#define CHUNK_SIZE_X 64
#define CHUNK_SIZE_Y 64
#define CHUNK_SIZE_Z 64

float kernel[5][5][5] = {
  {
    {1.0546170681737936e-05,0.00010989362203613103,0.00024002973835478245,0.00010989362203613103,1.0546170681737936e-05},
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
    {0.00024002973835478245,0.0025011673089900184,0.005463051664281514,0.0025011673089900184,0.00024002973835478245},
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
    {1.0546170681737936e-05,0.00010989362203613103,0.00024002973835478245,0.00010989362203613103,1.0546170681737936e-05},
  },
  {
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
    {0.0011451178374281624,0.011932401874651296,0.026062761849591155,0.011932401874651296,0.0011451178374281624},
    {0.0025011673089900184,0.026062761849591155,0.056926305563971345,0.026062761849591155,0.0025011673089900184},
    {0.0011451178374281624,0.011932401874651296,0.026062761849591155,0.011932401874651296,0.0011451178374281624},
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
  },
  {
    {0.00024002973835478245,0.0025011673089900184,0.005463051664281514,0.0025011673089900184,0.00024002973835478245},
    {0.0025011673089900184,0.026062761849591155,0.056926305563971345,0.026062761849591155,0.0025011673089900184},
    {0.005463051664281514,0.056926305563971345,0.12433848276956383,0.056926305563971345,0.005463051664281514},
    {0.0025011673089900184,0.026062761849591155,0.056926305563971345,0.026062761849591155,0.0025011673089900184},
    {0.00024002973835478245,0.0025011673089900184,0.005463051664281514,0.0025011673089900184,0.00024002973835478245},
  },
  {
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
    {0.0011451178374281624,0.011932401874651296,0.026062761849591155,0.011932401874651296,0.0011451178374281624},
    {0.0025011673089900184,0.026062761849591155,0.056926305563971345,0.026062761849591155,0.0025011673089900184},
    {0.0011451178374281624,0.011932401874651296,0.026062761849591155,0.011932401874651296,0.0011451178374281624},
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
  },
  {
    {1.0546170681737936e-05,0.00010989362203613103,0.00024002973835478245,0.00010989362203613103,1.0546170681737936e-05},
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
    {0.00024002973835478245,0.0025011673089900184,0.005463051664281514,0.0025011673089900184,0.00024002973835478245},
    {0.00010989362203613103,0.0011451178374281624,0.0025011673089900184,0.0011451178374281624,0.00010989362203613103},
    {1.0546170681737936e-05,0.00010989362203613103,0.00024002973835478245,0.00010989362203613103,1.0546170681737936e-05},
  },
};

int main(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Usage: vectorfield_smooth_32 <source zarr> <dest zarr> <zmin> <zmax>\n");
		printf("Use zmin and zmax to have different threads produce different parts of the output zarr\n");
		
		exit(-1);
	}
	
	int zmin = atoi(argv[3]);
	int zmax = atoi(argv[4]);
	
	int ymin = 0;
	int ymax = VOL_SIZE_Y;
	
    blosc2_init();

	printf("Opening ZARRs\n");
	
    ZARR_c128i1b16 *vfz = ZARROpen_c128i1b16(argv[1]);
    	
	ZARR_c64i1b1 *vfsz = ZARROpen_c64i1b1(argv[2]); // 64x64x64x4

	
    printf("Smoothing vector field...\n");
	
	int8_t vf[3];
	float vf_smooth[4];
	int8_t vfi_smooth[4];
    float min=0,max=0;
	
	for(int zc = zmin; zc < zmax; zc+=CHUNK_SIZE_Z)
	for(int yc = ymin; yc < ymax; yc+=CHUNK_SIZE_Y)
	for(int xc = 0; xc < VOL_SIZE_X; xc+=CHUNK_SIZE_X)
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
			
			for(int zo = z-SMOOTH_WINDOW; zo<=z+SMOOTH_WINDOW; zo++)
			{
  			  int zod=zo;
		      if (zo<0) zod = -zo;
			  if (zo>VOL_SIZE_Z-1) zod = 2*VOL_SIZE_Z-zo-1;
			  for(int yo = y-SMOOTH_WINDOW; yo<=y+SMOOTH_WINDOW; yo++)
			  {
			    int yod=yo;
		        if (yo<0) yod = -yo;
			    if (yo>VOL_SIZE_Y-1) yod = 2*VOL_SIZE_Y-yo-1;
			    for(int xo = x-SMOOTH_WINDOW; xo<=x+SMOOTH_WINDOW; xo++)
			    {
				  int xod=xo;
				
			      if (xo<0) xod = -xo;
				  if (xo>VOL_SIZE_X-1) xod = 2*VOL_SIZE_X-xo-1;
				
				  ZARRReadN_c128i1b16(vfz,zod+VOL_OFFSET_Z,yod+VOL_OFFSET_Y,xod+VOL_OFFSET_X,0,3,vf);
				
                  //printf("%d %d %d\n",(int)vf[0],(int)vf[1],(int)vf[2]);				
				  vf_smooth[0] += kernel[zod-z+SMOOTH_WINDOW][yod-y+SMOOTH_WINDOW][xod-x+SMOOTH_WINDOW]*vf[0];
				  vf_smooth[1] += kernel[zod-z+SMOOTH_WINDOW][yod-y+SMOOTH_WINDOW][xod-x+SMOOTH_WINDOW]*vf[1];
				  vf_smooth[2] += kernel[zod-z+SMOOTH_WINDOW][yod-y+SMOOTH_WINDOW][xod-x+SMOOTH_WINDOW]*vf[2];	  
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
				
			vfi_smooth[3] = ZARRRead_c128i1b16(vfz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,3);
					
			if (firstInChunk)
			{
				ZARRWriteN_c64i1b1(vfsz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,0,4,vfi_smooth);
				firstInChunk = false;
			}
			else
			{
				ZARRNoCheckWriteN_c64i1b1(vfsz,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X,0,4,vfi_smooth);
			}
        }			
	}

	printf("Finished, closing ZARRs\n");
	
	ZARRClose_c128i1b16(vfz);
	ZARRClose_c64i1b1(vfsz);
	
	printf("Calling blosc2_destroy\n");
	
	blosc2_destroy();
	
	printf("min=%f\n",min);
	printf("max=%f\n",max);
    return 0;
}

