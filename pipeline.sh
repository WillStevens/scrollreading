#!/bin/sh

XYZ=03912_04200_05800

#mkdir ../construct/s$XYZ
#mkdir ../construct/s$XYZ/nonint
#mkdir ../construct/s$XYZ/nonint/output

#./scrollprocess D:/construct/$XYZ ../construct/s$XYZ

#for FILE in ../construct/s${XYZ}/v*.csv
#do
#  echo "Running holefiller on $FILE"
#  ./holefiller $FILE ../construct/s${XYZ}/nonint 1 0 0 0 0 1
#done

#./render D:/construct/$XYZ ../construct/s$XYZ/nonint
 
#./jigsaw2 ../construct/s$XYZ/nonint

./render D:/construct/$XYZ ../construct/s$XYZ/nonint/output
