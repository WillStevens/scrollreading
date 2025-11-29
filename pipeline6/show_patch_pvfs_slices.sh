mkdir d:/temp/patches_$1
./interpolate d:/pipelineOutput/patch_$1.bin d:/pipelineOutput/patch_$1_i.bin
echo `./patch_extent d:/pipelineOutput/patch_$1_i.bin`

extent=`./patch_extent d:/pipelineOutput/patch_$1_i.bin`
arrext=(${extent//,/ })

echo ${arrext[0]}
echo ${arrext[1]}

arrext[0]=`printf "%.0f" ${arrext[0]}`
arrext[1]=`printf "%.0f" ${arrext[1]}`
arrext[2]=`printf "%.0f" ${arrext[2]}`
arrext[3]=`printf "%.0f" ${arrext[3]}`
arrext[4]=`printf "%.0f" ${arrext[4]}`
arrext[5]=`printf "%.0f" ${arrext[5]}`


echo $((${arrext[3]} - ${arrext[0]}))
echo $((${arrext[4]} - ${arrext[1]}))

for ((z=${arrext[2]}; z<${arrext[5]}; z+=10))
do
  i=`printf "%05d" $(( ( $z - ${arrext[2]} ) / 10 ))` 
 ./zarr_show2_64 d:/vf_zarrs/s4/pvfs_2025_11_17.zarr ${arrext[0]} ${arrext[1]} $z $((${arrext[3]}-${arrext[0]})) $((${arrext[4]}-${arrext[1]})) d:/temp/patches_$1/tmp_pvfs_$i.tif d:/pipelineOutput/patch_$1_i.bin
done

/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -i d:/temp/patches_$1/tmp_pvfs_%05d.tif d:/temp/patches_$1/patch_$1.gif

SEED_X=1798
SEED_Y=1161
SEED_Z=5120
