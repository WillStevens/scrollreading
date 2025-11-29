
for ((z=4096; z<6144; z+=128))
do
 ./zarr_show2_64 d:/vf_zarrs/s4/pvfs_2025_11_03.zarr 640 640 $z 2048 2048 d:/vf_zarrs/s4/images/pvfs_2025_11_03.zarr/z$z.tif
done
