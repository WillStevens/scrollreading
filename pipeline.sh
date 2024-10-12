#!/bin/sh

XYZ=03400_04200_07336

mkdir ../construct/s$XYZ
mkdir ../construct/s$XYZ/nonint
mkdir ../construct/s$XYZ/nonint/output

./scrollprocess ../construct/$XYZ ../construct/s$XYZ

for FILE in ../construct/s$XYZ/v*.csv
do
  echo "Running holefiller on $FILE"
  ./holefiller $FILE ../construct/s$XYZ/nonint 1 0 0 0 0 1
done

./render ../construct/$XYZ ../construct/s$XYZ/nonint
 
python jigsaw.py ../construct/s$XYZ/nonint