#!/bin/sh

XYZ=02512_03476_01500

./render D:/construct/$XYZ ../debug -6 1 0 0 0 0 1

for NUMBER in 1556 3894 3147 3036 2829 2629 2617 2210 2026 1765 1741 1656
do
  ./interpolate ../debug/v4_${NUMBER}_-6.tif ../debug/v4_${NUMBER}_-6.rnd ../debug/v4_${NUMBER}_interp.csv 
  cat ../debug/v4_${NUMBER}.csv ../debug/v4_${NUMBER}_interp.csv > ../debug/out/v4_${NUMBER}.csv
done

./render D:/construct/$XYZ ../debug/out -8 1 0 0 0 0 1
./render D:/construct/$XYZ ../debug/out -6 1 0 0 0 0 1
./render D:/construct/$XYZ ../debug/out -4 1 0 0 0 0 1
./render D:/construct/$XYZ ../debug/out -2 1 0 0 0 0 1
