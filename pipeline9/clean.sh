OUTPUT_DIR=c:/vesuvius/output/PHerc0125

mkdir $OUTPUT_DIR
mkdir $OUTPUT_DIR/surface.bp
mkdir $OUTPUT_DIR/surface.bp/surface
mkdir $OUTPUT_DIR/boundary.bp
mkdir $OUTPUT_DIR/boundary.bp/surface
mkdir $OUTPUT_DIR/patches

cd $OUTPUT_DIR/surface.bp/surface
rm *.*.*

cd $OUTPUT_DIR/boundary.bp/surface
rm *.*.*

cd $OUTPUT_DIR/patches
rm patch_*.bin

