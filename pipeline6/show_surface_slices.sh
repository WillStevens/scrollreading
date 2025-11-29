for ((z=4800; z<5700; z+=10))
do
 
 ./zarr_show2_u8 d:/zarrs/s4_059_medial_ome.zarr/0 1100 850 $z 1024 768 d:/temp/examine/tmps_$z.tif d:/pipelineOutput/patch_132_i.bin d:/pipelineOutput/patch_151_i.bin d:/pipelineOutput/patch_56_i.bin d:/pipelineOutput/patch_61_i.bin d:/pipelineOutput/patch_140_i.bin


done
