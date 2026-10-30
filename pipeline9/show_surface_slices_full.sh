for ((z=2000; z<9000; z+=2000))
do
 
 ../pipeline6/zarr_show2_u8 d:/zarrs/s4_059_medial_ome.zarr/0 0 0 $z 3300 3300 d:/pipelineOutputS4/compare/tmp2_$z.tif d:/pipelineOutputS4/all_s4_06/patch_7.bin d:/pipelineOutputS4/all_s4_06/patch_9.bin

done
