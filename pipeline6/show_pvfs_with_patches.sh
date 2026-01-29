OUTPUT_DIR=d:/s4_explore/sliceanim
PIPELINE_DIR=d:/pipelineOutputS4_2025_12_16
ZARR_DIR=d:/test2.zarr

#/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -f concat -safe 0 -i $OUTPUT_DIR/list.txt -c copy $OUTPUT_DIR/slices_join.gif

/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -i $OUTPUT_DIR/slices_join.gif -b:v 1M $OUTPUT_DIR/slices_join.mp4

exit

arrext[0]=2461
arrext[1]=926
arrext[2]=3796
arrext[3]=3000
arrext[4]=1548
arrext[5]=4396

echo $((${arrext[3]} - ${arrext[0]}))
echo $((${arrext[4]} - ${arrext[1]}))

for ((z=${arrext[2]}; z<${arrext[5]}; z+=5))
do
  i=`printf "%05d" $(( ( $z - ${arrext[2]} ) / 5 ))` 
  ./zarr_show2_64 $ZARR_DIR ${arrext[0]} ${arrext[1]} $z $((${arrext[3]}-${arrext[0]})) $((${arrext[4]}-${arrext[1]})) $OUTPUT_DIR/slice_$i.tif $PIPELINE_DIR/surface.bp
done

/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -r 5 -i $OUTPUT_DIR/slice_%05d.tif $OUTPUT_DIR/slices.gif

