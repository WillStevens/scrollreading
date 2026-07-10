#include <iostream>
#include <stdint.h>

#include "calcvectorfield.h"
#include "zarr_c64i1b256.h"
#include "parameters.h"

int main(void)
{
	VectorFieldCalculator vfc("d:/zarrs/s4_059_medial_ome.zarr/0");
	Vec3 v;
	
	vfc.GetSmoothedVectorField(1800,1161,5120,v);

	std::cout << v.x << " " << v.y << " " << v.z << std::endl;
	
	int8_t b[3];
	ZARR_c64i1b256 *za = ZARROpen_c64i1b256("d:/vf_zarrs/s4/pvfs_2025_12_16.zarr");
	ZARRReadN_c64i1b256(za,5120,1161,1800,0,3,b);
	ZARRClose_c64i1b256(za);
	
	std::cout << ((float)b[0])/127 << " " << ((float)b[1])/127 << " " << ((float)b[2])/127 << std::endl;
}
