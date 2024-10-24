#!/bin/sh

XYZ=03400_04200_05800

#mkdir ../construct/s$XYZ
#mkdir ../construct/s$XYZ/nonint
#mkdir ../construct/s$XYZ/nonint/output

#./scrollprocess D:/construct/$XYZ ../construct/s$XYZ 0 -1 0

#for FILE in ../construct/s${XYZ}/v*.csv
#do
#  echo "Running holefiller on $FILE"
#  ./holefiller $FILE ../construct/s${XYZ}/nonint 1 0 0 0 0 1
#done

#./render D:/construct/$XYZ ../construct/s$XYZ/nonint 1 0 0 0 0 1
 
./jigsaw2 ../construct/s$XYZ/nonint 1 0 0 0 0 1

./render D:/construct/$XYZ ../construct/s$XYZ/nonint/output 1 0 0 0 0 1
