for ((z=4096; z<4096+2048; z+=10))
do
 
 ./zarr_show2_c128 d:/output_zarrs/s4_filtered 1150 1150 $z 1024 1024 d:/s4_explore/slices/anded/s4_anded_$z.tiff

done
