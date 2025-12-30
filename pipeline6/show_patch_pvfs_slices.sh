WORKING_DIR=d:/s4_explore
PIPELINE_DIR=d:/pipelineOutputS4_2025_12_16
ZARR_DIR=d:/test2.zarr

mkdir $WORKING_DIR/patches_$1
./interpolate $PIPELINE_DIR/patch_$1.bin $PIPELINE_DIR/patch_$1_i.bin
echo `./patch_extent $PIPELINE_DIR/patch_$1_i.bin`

extent=`./patch_extent $PIPELINE_DIR/patch_$1_i.bin`
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
 ./zarr_show2_64 $ZARR_DIR ${arrext[0]} ${arrext[1]} $z $((${arrext[3]}-${arrext[0]})) $((${arrext[4]}-${arrext[1]})) $WORKING_DIR/patches_$1/tmp_pvfs_$i.tif $PIPELINE_DIR/patch_$1_i.bin
done

/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -i $WORKING_DIR/patches_$1/tmp_pvfs_%05d.tif $WORKING_DIR/patches_$1/patch_$1.gif

