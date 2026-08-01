#!/usr/bin/env bash

# Usage: ./convert.sh /path/to/source_folder /path/to/destination_folder /path/to/destination_folder_bad list
SRC_FOLDER=$1
DEST_FOLDER=$2
BAD_FOLDER=$3
LIST_FILE=$4

# --- customize these ---
SEARCH=": \[1, 1\]"
REPLACE=": [0.25, 0.25]"
# -----------------------

mkdir -p $DEST_FOLDER
mkdir -p $BAD_FOLDER

# Load the list of integers into an associative array for O(1) lookup
declare -A wanted
while IFS= read -r num; do
      num="${num%$'\r'}"
    [[ -n "$num" ]] && wanted["$num"]=1
done < $LIST_FILE

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
	./bin2tifxyz $bin_file $sub_folder --uuid WSP_$base_name --step 1 1

    sed -i "s/${SEARCH}/${REPLACE}/g" $sub_folder/meta.json

	if [[ -n "${wanted["$num"]}" ]]; then
        echo "Moved bad patch: $sub_folder -> $BAD_FOLDER/"
        mv $sub_folder $BAD_FOLDER
    fi
done

echo "Done."