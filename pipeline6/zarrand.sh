#!/bin/sh

if [[ $# -ne 5 ]]; then
  echo "Usage: $0 <source1 dir> <source2 dir> <dest dir> <num processes> <this process>" >&2
  exit 1
fi 

SOURCE1="$1"
SOURCE2="$2"
DEST="$3"
NUM_PROC="$4"
THIS_PROC="$5"

if [[ ! -d "$SOURCE1" ]]; then
  echo "Error: '$SOURCE1' is not a directory or does not exist" >&2
  exit 1
fi

if [[ ! -d "$SOURCE2" ]]; then
  echo "Error: '$SOURCE2' is not a directory or does not exist" >&2
  exit 1
fi

if [[ ! -d "$DEST" ]]; then
  echo "Error: '$DEST' is not a directory or does not exist" >&2
  exit 1
fi

for file in "$SOURCE1"/*; do
  #Split the file into its three components
  filename=$(basename "$file")
  arr=(${filename//./ })
  echo "${arr[0]}"
  echo "${arr[1]}"
  echo "${arr[2]}"
  
  if (( ${arr[2]} % $NUM_PROC == $THIS_PROC )); then
    echo $file
    source2file=$SOURCE2/${arr[0]}/${arr[1]}/${arr[2]}
    echo $source2file
    destfile=$DEST/$filename
    echo $destfile
    ./zarrand $file $source2file $destfile
  fi
done
