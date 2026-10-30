#!/usr/bin/env bash

# Usage: ./convert.sh /path/to/source_folder /path/to/destination_folder 
SRC_FOLDER=$1
DEST_FOLDER=$2

# --- customize these ---
SEARCH=": \[1, 1\]"
REPLACE=": [0.25, 0.25]"
# -----------------------

mkdir -p $DEST_FOLDER

shopt -s nullglob
for bin_file in $SRC_FOLDER/*.bin; do
    base_name=$(basename $bin_file .bin)
    sub_folder=$DEST_FOLDER/$base_name
    num="${base_name#patch_}"
	
    echo "Processing: $bin_file"
    mkdir -p $sub_folder

	# Assumes bin2csv writes the .csv next to the .bin file (same base name).
    #csv_file=$SRC_FOLDER/${base_name}.csv

    #./bin2csv $bin_file > $csv_file
	./bin2tifxyz $bin_file $sub_folder --uuid WSP20260901_$base_name --step 1 1

    sed -i "s/${SEARCH}/${REPLACE}/g" $sub_folder/meta.json

done

echo "Done."