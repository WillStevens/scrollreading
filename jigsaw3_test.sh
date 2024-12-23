#!/bin/sh

# WS 2024-12-02 Improvements to pipeline, focussed on scroll 4
# Improvements made:
#   Specify X,Y & Z coordinates of cube to work on
#   Pipeline will extract the cube from the scroll images (extract_general.py)
#   It will also work out the direction to the umbilicus and the plane and normal vectors (calc_surface_and_normal.py)

IMAGES_DIR=d:/scroll4
CUBES_DIR=d:/scroll4_cubes/
CONSTRUCT_DIR=d:/scroll4_construct/

X_START=01722
Y_START=02200
Z_START=05511

#IMAGES_DIR=d:/vesuvius
#CUBES_DIR=d:/construct/
#CONSTRUCT_DIR=C:/Users/Will/Downloads/vesuvius/construct/

#X_START=03988
#Y_START=02512
#Z_START=01500

SIZE=512

YXZ=${Y_START}_${X_START}_${Z_START}

# These define the projection plane vector and normal in the direction toward scroll umbilicus
# The magnitude of each vector must be 1000
P00=$(python calc_surface_and_normal.py 0 0 $X_START $Y_START $Z_START $SIZE)
P01=$(python calc_surface_and_normal.py 0 1 $X_START $Y_START $Z_START $SIZE)
P02=$(python calc_surface_and_normal.py 0 2 $X_START $Y_START $Z_START $SIZE)

P10=$(python calc_surface_and_normal.py 1 0 $X_START $Y_START $Z_START $SIZE)
P11=$(python calc_surface_and_normal.py 1 1 $X_START $Y_START $Z_START $SIZE)
P12=$(python calc_surface_and_normal.py 1 2 $X_START $Y_START $Z_START $SIZE)

P20=$(python calc_surface_and_normal.py 2 0 $X_START $Y_START $Z_START $SIZE)
P21=$(python calc_surface_and_normal.py 2 1 $X_START $Y_START $Z_START $SIZE)
P22=$(python calc_surface_and_normal.py 2 2 $X_START $Y_START $Z_START $SIZE)

#P00=1000
#P01=0
#P02=0

#P10=0
#P11=0
#P12=1000

#P20=0
#P21=-1000
#P22=0

echo $YXZ $P00 $P01 $P02 $P10 $P11 $P12 $P20 $P21 $P22

./jigsaw3 $CONSTRUCT_DIR/s$YXZ/nonint $P00 $P01 $P02 $P10 $P11 $P12
#./jigsaw3 C:/Users/Will/Downloads/vesuvius/jigsaw3_test $P00 $P01 $P02 $P10 $P11 $P12

#./render D:/construct/$YXZ ../construct/s$YXZ/nonint/output_jigsaw3 -6 $P00 $P01 $P02 $P10 $P11 $P12
./render $CUBES_DIR/$YXZ $CONSTRUCT_DIR/s$YXZ/nonint/output_jigsaw3 -6 $P00 $P01 $P02 $P10 $P11 $P12
