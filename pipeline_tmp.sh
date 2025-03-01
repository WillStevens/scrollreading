#!/bin/sh

IMAGES_DIR=d:/vesuvius
CUBES_DIR=d:/scroll1_cubes/
CONSTRUCT_DIR=d:/scroll1_construct/
SURFACE_DIR=d:/scroll1_surfaces/

X_START=02966
Y_START=02512
Z_START=02522

SIZE=512

YXZ=${Y_START}_${X_START}_${Z_START}

python extract_general.py $IMAGES_DIR $CUBES_DIR $X_START $Y_START $Z_START $SIZE

if false; then

# These define the projection plane vector and normal in the direction toward scroll umbilicus
# The magnitude of each vector must be 1000
P00=$(python calc_surface_and_normal_scroll1.py 0 0 $X_START $Y_START $Z_START $SIZE)
P01=$(python calc_surface_and_normal_scroll1.py 0 1 $X_START $Y_START $Z_START $SIZE)
P02=$(python calc_surface_and_normal_scroll1.py 0 2 $X_START $Y_START $Z_START $SIZE)

P10=$(python calc_surface_and_normal_scroll1.py 1 0 $X_START $Y_START $Z_START $SIZE)
P11=$(python calc_surface_and_normal_scroll1.py 1 1 $X_START $Y_START $Z_START $SIZE)
P12=$(python calc_surface_and_normal_scroll1.py 1 2 $X_START $Y_START $Z_START $SIZE)

P20=$(python calc_surface_and_normal_scroll1.py 2 0 $X_START $Y_START $Z_START $SIZE)
P21=$(python calc_surface_and_normal_scroll1.py 2 1 $X_START $Y_START $Z_START $SIZE)
P22=$(python calc_surface_and_normal_scroll1.py 2 2 $X_START $Y_START $Z_START $SIZE)

echo $YXZ $P00 $P01 $P02 $P10 $P11 $P12 $P20 $P21 $P22


#exit 1

# Example values for a cube that lies in the +y direction of the umbilicus
P00=1000
P01=0
P02=0

P10=0
P11=0
P12=1000

P20=0
P21=-1000
P22=0

#P00=866
#P01=-500
#P02=0

#P10=-250
#P11=-433
#P12=866

#P20=-433
#P21=-750
#P22=-500

P00=981
P01=190
P02=0

P10=0
P11=0
P12=1000

P20=190
P21=-981
P22=0
 
 
mkdir $CONSTRUCT_DIR/s$YXZ
mkdir $CONSTRUCT_DIR/s$YXZ/nonint_sj
mkdir $CONSTRUCT_DIR/s$YXZ/nonint_sj/output_jigsaw3

# Need to build holefiller with projection plane vectors as compile-time constants
g++ -O3 -Wall -DPLANEVECTORS_0_0=$P00 -DPLANEVECTORS_0_1=$P01 -DPLANEVECTORS_0_2=$P02 -DPLANEVECTORS_1_0=$P10 -DPLANEVECTORS_1_1=$P11 -DPLANEVECTORS_1_2=$P12 -DPLANEVECTORS_2_0=$P20 -DPLANEVECTORS_2_1=$P21 -DPLANEVECTORS_2_2=$P22 holefiller.c -o holefiller.exe

#./scrollprocess $CUBES_DIR/$YXZ $CONSTRUCT_DIR/s$YXZ $P20 $P21 $P22

for FILE in $SURFACE_DIR/${YXZ}/*.csv
do
  echo "Running holefiller on $FILE"
  ./holefiller $FILE $CONSTRUCT_DIR/s${YXZ}/nonint_sj $P00 $P01 $P02 $P10 $P11 $P12
done

#./render $CUBES_DIR/$YXZ $CONSTRUCT_DIR/s$YXZ/nonint $P00 $P01 $P02 $P10 $P11 $P12
 
./jigsaw3 $CONSTRUCT_DIR/s$YXZ/nonint_sj $P00 $P01 $P02 $P10 $P11 $P12

fi

./render $CUBES_DIR/$YXZ $CONSTRUCT_DIR/s$YXZ/nonint_sj/output_jigsaw3 -6 $P00 $P01 $P02 $P10 $P11 $P12
