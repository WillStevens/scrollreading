WORKING_DIR=d:/s4_explore
PIPELINE_DIR=d:/pipelineOutputS4_2025_12_16
ZARR_DIR=d:/test2.zarr

mkdir $WORKING_DIR/patches_$1_$2
./interpolate $PIPELINE_DIR/patch_$1.bin $PIPELINE_DIR/patch_$1_i.bin
echo `./patch_extent $PIPELINE_DIR/patch_$1_i.bin`
./interpolate $PIPELINE_DIR/patch_$2.bin $PIPELINE_DIR/patch_$2_i.bin
echo `./patch_extent $PIPELINE_DIR/patch_$2_i.bin`

extent1=`./patch_extent $PIPELINE_DIR/patch_$1_i.bin`
arrext1=(${extent1//,/ })

echo ${arrext1[0]}
echo ${arrext1[1]}

arrext1[0]=`printf "%.0f" ${arrext1[0]}`
arrext1[1]=`printf "%.0f" ${arrext1[1]}`
arrext1[2]=`printf "%.0f" ${arrext1[2]}`
arrext1[3]=`printf "%.0f" ${arrext1[3]}`
arrext1[4]=`printf "%.0f" ${arrext1[4]}`
arrext1[5]=`printf "%.0f" ${arrext1[5]}`

extent2=`./patch_extent $PIPELINE_DIR/patch_$2_i.bin`
arrext2=(${extent2//,/ })

echo ${arrext2[0]}
echo ${arrext2[1]}

arrext2[0]=`printf "%.0f" ${arrext2[0]}`
arrext2[1]=`printf "%.0f" ${arrext2[1]}`
arrext2[2]=`printf "%.0f" ${arrext2[2]}`
arrext2[3]=`printf "%.0f" ${arrext2[3]}`
arrext2[4]=`printf "%.0f" ${arrext2[4]}`
arrext2[5]=`printf "%.0f" ${arrext2[5]}`

arrext[0]=$((arrext1[0]<arrext2[0]?arrext1[0]:arrext2[0]))
arrext[1]=$((arrext1[1]<arrext2[1]?arrext1[1]:arrext2[1]))
arrext[2]=$((arrext1[2]<arrext2[2]?arrext1[2]:arrext2[2]))
arrext[3]=$((arrext1[3]>arrext2[3]?arrext1[3]:arrext2[3]))
arrext[4]=$((arrext1[4]>arrext2[4]?arrext1[4]:arrext2[4]))
arrext[5]=$((arrext1[5]>arrext2[5]?arrext1[5]:arrext2[5]))

echo $((${arrext[3]} - ${arrext[0]}))
echo $((${arrext[4]} - ${arrext[1]}))

for ((z=${arrext[2]}; z<${arrext[5]}; z+=10))
do
  i=`printf "%05d" $(( ( $z - ${arrext[2]} ) / 10 ))` 
# ./zarr_show2_64 $ZARR_DIR ${arrext[0]} ${arrext[1]} $z $((${arrext[3]}-${arrext[0]})) $((${arrext[4]}-${arrext[1]})) $WORKING_DIR/patches_$1_$2/tmp_pvfs_$i.tif $PIPELINE_DIR/patch_$1_i.bin $PIPELINE_DIR/patch_$2_i.bin
  ./zarr_show2_64 $ZARR_DIR ${arrext[0]} ${arrext[1]} $z $((${arrext[3]}-${arrext[0]})) $((${arrext[4]}-${arrext[1]})) $WORKING_DIR/patches_$1_$2/tmp_pvfs_$i.tif
done

/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -i $WORKING_DIR/patches_$1_$2/tmp_pvfs_%05d.tif $WORKING_DIR/patches_$1_$2/patch_$1_$2.gif

