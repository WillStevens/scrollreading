#!/bin/sh

if [[ $# -ne 4 ]]; then
  echo "Usage: $0 <source1 dir> <dest dir> <num processes> <this process>" >&2
  exit 1
fi 

SOURCE1="$1"
DEST="$2"
NUM_PROC="$3"
THIS_PROC="$4"

if [[ ! -d "$SOURCE1" ]]; then
  echo "Error: '$SOURCE1' is not a directory or does not exist" >&2
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
    destfile=$DEST/$filename
    echo $destfile
    ./zarrconv $file $destfile
  fi
done
