
for ((z=4106; z<4096+2048; z+=10))
do
  i=`printf "%05d" $(( ( $z - 4106 ) / 10 ))` 

 ./zarr_show2_64 d:/vf_zarrs/s4/pvfs_2025_11_17.zarr 640 640 $z 2048 2048 d:/temp/whole/fixed3_pvfs_$i.tif d:/pipelineOutput/surface.bp
done
