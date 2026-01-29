/*
 Will Stevens, August 2024
 
 Routines for processing cubic volumes of the Herculanium Papyri.
  
 Released under GNU Public License V3
*/

#include <string> 

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <dirent.h>

#include "zarr_c128.c"


int main(int argc, char *argv[])
{		
    ZARR_c128 *outputZarr = ZARROpen_c128("d:\\tmp_zarr");

	uint8_t x = ZARRRead_c128(outputZarr,1282,1282,1282);
	ZARRWrite_c128(outputZarr,1282,1282,1282,x);
	
    ZARRClose_c128(outputZarr);		
}

