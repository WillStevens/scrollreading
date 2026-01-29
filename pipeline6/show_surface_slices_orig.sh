for ((z=4096; z<4096+2048; z+=10))
do
 
 ./zarr_show2_u8 d:/zarrs/s4_059_medial_ome.zarr/0 1150 1150 $z 1024 1024 d:/s4_explore/slices/orig/s4_orig_$z.tiff

done
