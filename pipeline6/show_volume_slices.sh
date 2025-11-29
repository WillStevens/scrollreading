for ((z=4096; z<4096+2048; z+=10))
do
 

 ./zarr_show2_u16 d:/zarrs/s4/20231117161658.zarr/0 640 640 $z 2048 2048 d:/temp/whole/tmp_$z.tif d:/pipelineOutput/surface.bp

done
