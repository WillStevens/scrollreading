#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "tiffio.h"

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SIZE_X 256
#define SIZE_Y 256
#define SIZE_Z 256

#define SURFACE_VALUE 255

void WriteSliceTiff(unsigned char ***volume,int s)
{
	if (1==1)
	{
		char tiffName[256];
		
		sprintf(tiffName,"tmp//slice_%04d.tif",s);

		TIFF *tif = TIFFOpen(tiffName,"w");
		
		if (tif)
		{
			tdata_t buf;
			uint32_t row;
			
			TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, SIZE_X); 
			TIFFSetField(tif, TIFFTAG_IMAGELENGTH, SIZE_Y); 
			TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8); 
			TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1); 
			TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);   
			TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
			TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
			TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
			TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
			
			buf = _TIFFmalloc(SIZE_X);
			for(row=0; row<SIZE_Y;row++)
			{
				for(int i=0; i<SIZE_X; i++)
				{
				   ((uint8_t *)buf)[i] = volume[s][row][i];
				}
				TIFFWriteScanline(tif,buf,row,0);
			}
			_TIFFfree(buf);
		}
		
		TIFFClose(tif);

	}
}

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


unsigned char ***MakeU8Array(int x, int y, int z)
{
	unsigned char ***zarray = malloc(z*sizeof(unsigned char**));
	
	for(int zi=0; zi<z; zi++)
	{
	  zarray[zi] = malloc(y*sizeof(unsigned char*));
	  for(int yi=0; yi<y; yi++)
	  {
		  zarray[zi][yi] = malloc(x*sizeof(unsigned char));
		  for(int xi=0; xi<x; xi++)
		  {
			  zarray[zi][yi][xi] = 0;
		  }
	  }
	}
	
	return zarray;
}

int main() {

	unsigned char ***surface = MakeU8Array(SIZE_Z,SIZE_Y,SIZE_X);
	
	FILE *f = fopen("test_switchdetect.csv","r");
	
	int v;
	
    for(int z = 0; z<SIZE_Z; z++)
	{
        printf("%d\n",z);
        for(int y = 0; y<SIZE_Y; y++)
        for(int x = 0; x<SIZE_X; x++)
		{
			fscanf(f,"%d\n",&v);
			surface[z][y][x] = v;
		}
		
		WriteSliceTiff(surface,z);
	}
	
	fclose(f);
	
	int vf_dims[3] = {SIZE_Z-2*VF_RAD,SIZE_Y-2*VF_RAD,SIZE_X-2*VF_RAD};
	
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
			distance[z][y][x]=100.0; // This just means 'large'
			
            if (surface[z+VF_RAD][y+VF_RAD][x+VF_RAD] != SURFACE_VALUE)
			{
                int minCount = 0;
                float lastDist = 0;
                for(int i = 0; i<SORTED_DIST_SIZE; i++)
				{
                    if (surface[z+VF_RAD+(int)sortedDistances[i][3]][y+VF_RAD+(int)sortedDistances[i][2]][x+VF_RAD+(int)sortedDistances[i][1]] == SURFACE_VALUE)
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
			else
			{
			    distance[z][y][x]=0.0;
			}
		}
	}

    printf("Smoothing vector field...\n");

	float vf_smooth[3];
	
	f = fopen("test_switchdetect_smooth.csv","w");

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