OUTPUT_DIR=d:/s4_explore/slices

/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -r 5 -f concat -safe 0 -i $OUTPUT_DIR/anded/files.txt $OUTPUT_DIR/anded.mp4


/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -r 5 -f concat -safe 0 -i $OUTPUT_DIR/orig/files.txt $OUTPUT_DIR/orig.mp4


/cygdrive/c/users/will/downloads/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg -i $OUTPUT_DIR/orig.mp4 -i $OUTPUT_DIR/anded.mp4 -filter_complex "hstack=inputs=2" $OUTPUT_DIR/both.mp4 