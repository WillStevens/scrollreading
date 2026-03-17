#include <cstdio>

#include "parameters.h"

#include "bigpatch.h"
#include "patch_generator.h"

int main(void)
{
	printf("Started\n");
    fflush(stdout);
	
	PatchGenerator *pg = new PatchGenerator(string(SURFACE_ZARR),string(VECTORFIELD_ZARR));
	
	float seed[9] = {
      SEED_X,
	  SEED_Y,
	  SEED_Z,
	  SEED_AXIS1_X,
	  SEED_AXIS1_Y,
	  SEED_AXIS1_Z,
	  SEED_AXIS2_X,
	  SEED_AXIS2_Y,
	  SEED_AXIS2_Z};

	BigPatch *bp = OpenBigPatch("temp.bp");
	BigPatch *bpb = OpenBigPatch("tempb.bp");
	
	for(int i=0; i<10; i++)
	{
		Patch patch,boundary;
		
		/*Remove seed from boundary*/
	    pg->GeneratePatch(seed,patch,boundary);
		
	
        if (i==0)
		{
			AddToBigPatch(bp,patch,i);
			AddToBigPatch(bpb,boundary,i);
		}			
		else
		{
		}
	}
	
	CloseBigPatch(bpb);
	CloseBigPatch(bp);
}

