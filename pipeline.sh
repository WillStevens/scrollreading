#!/bin/sh

XYZ=02512_03988_01500
# These define the projection plane vector and normal to that in the direction toward scroll umbilicus
# The magnitude of each vector must be 1000
P00=1000
P01=0
P02=0

P10=0
P11=0
P12=1000

P20=0
P21=-1000
P22=0

mkdir ../construct/s$XYZ
mkdir ../construct/s$XYZ/nonint
mkdir ../construct/s$XYZ/nonint/output

# Need to build holefiller with projection plane vectors as compile-time constants
g++ -O3 -Wall -DPLANEVECTORS_0_0=$P00 -DPLANEVECTORS_0_1=$P01 -DPLANEVECTORS_0_2=$P02 -DPLANEVECTORS_1_0=$P10 -DPLANEVECTORS_1_1=$P11 -DPLANEVECTORS_1_2=$P12 -DPLANEVECTORS_2_0=$P20 -DPLANEVECTORS_2_1=$P21 -DPLANEVECTORS_2_2=$P22 holefiller.c -o holefiller.exe

./scrollprocess D:/construct/$XYZ ../construct/s$XYZ $P20 $P21 $P22

for FILE in ../construct/s${XYZ}/v*.csv
do
  echo "Running holefiller on $FILE"
  ./holefiller $FILE ../construct/s${XYZ}/nonint $P00 $P01 $P02 $P10 $P11 $P12
done

./render D:/construct/$XYZ ../construct/s$XYZ/nonint $P00 $P01 $P02 $P10 $P11 $P12
 
./jigsaw2 ../construct/s$XYZ/nonint $P00 $P01 $P02 $P10 $P11 $P12

./render D:/construct/$XYZ ../construct/s$XYZ/nonint/output -6 $P00 $P01 $P02 $P10 $P11 $P12
