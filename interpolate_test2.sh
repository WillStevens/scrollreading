#!/bin/sh

XYZ=02512_03988_01500

./render D:/construct/$XYZ ../debug2 -6 1 0 0 0 0 1

for NUMBER in v5_1085 v5_1101 v5_1193 v5_1417 v5_1852 v5_1931 v5_2040 v5_2127 v5_2146 v5_2197 v5_2275 v5_2315 v5_2319 v5_2592 v5_2782 v5_2825 v5_2876 v5_2962 v5_3040 v5_3041
do
  ./interpolate ../debug2/${NUMBER}_-6.tif ../debug2/${NUMBER}_-6.rnd ../debug2/${NUMBER}_interp.csv 
  cat ../debug2/${NUMBER}.csv ../debug2/${NUMBER}_interp.csv > ../debug2/out/${NUMBER}.csv
done

./render D:/construct/$XYZ ../debug2/out -8 1 0 0 0 0 1
./render D:/construct/$XYZ ../debug2/out -6 1 0 0 0 0 1
./render D:/construct/$XYZ ../debug2/out -4 1 0 0 0 0 1
./render D:/construct/$XYZ ../debug2/out -2 1 0 0 0 0 1
