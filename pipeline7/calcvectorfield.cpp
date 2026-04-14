#include "calcvectorfield.h"

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


VectorFieldCalculator::VectorFieldCalculator(const std::string &zarrName)
{
    // Precompute nearest voxels and associated distance and direction vector
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
	
	
  surfaceZarr = ZARROpen_1(zarrName.c_str());
}

VectorFieldCalculator::~VectorFieldCalculator()
{
	ZARRClose_1(surfaceZarr);
}

void VectorFieldCalculator::GetVectorField(int x, int y, int z, Vec3 &v)
{
    uint8_t foundValue = ZARRRead_1(surfaceZarr,z+VOL_OFFSET_Z,y+VOL_OFFSET_Y,x+VOL_OFFSET_X);
	Vec3 vf;
	
	{
        int minCount = 0;
        float lastDist = 0;
		int i=0;
        for(; i<SORTED_DIST_SIZE; i++)
		{ 
            if (ZARRRead_1(surfaceZarr,z+sortedDistances[i][3]+VOL_OFFSET_Z,y+sortedDistances[i][2]+VOL_OFFSET_Y,x+sortedDistances[i][1]+VOL_OFFSET_X) != foundValue)
			{

                if (sortedDistances[i][0] != lastDist)
				{
                    if (minCount>0)
					{
						/* If we're in a surface voxel, then point the vector away from the edge of the surface, but
						   if we're not in a surface voxel point it towards the surface */
                        v = (vf/minCount) * (foundValue==0?1.0f:-1.0f);
					}
        
					lastDist = sortedDistances[i][0];
				}
                
				vf.x += sortedDistances[i][4];
                vf.y += sortedDistances[i][5];
                vf.z += sortedDistances[i][6];
                minCount += 1;
			}
		}
			
		if (i==SORTED_DIST_SIZE && minCount>0)
		{
			/* If we're in a surface voxel, then point the vector away from the edge of the surface, but
			   if we're not in a surface voxel point it towards the surface */
            v = (vf/minCount) * (foundValue==0?1.0f:-1.0f);
    		/* we need value to be zero in a surface and 255 around the boundaries of it */
		}
	}
}

void VectorFieldCalculator::GetSmoothedVectorField(int x, int y, int z, Vec3 &v)
{
	v.x=v.y=v.z=0.0;
	
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
				
		  Vec3 vf;
		  GetVectorField(x,y,z,vf);
				
		  v += kernel[zod-z+SMOOTH_WINDOW][yod-y+SMOOTH_WINDOW][xod-x+SMOOTH_WINDOW]*vf;
	    }
	  }
	}
							
	v = v*GAUSS_SCALE;
}
