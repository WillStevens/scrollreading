# Parameters shared by several programs
OUTPUT_DIR="d:/pipelineOutput"
VOL_OFFSET_X=0
VOL_OFFSET_Y=0
VOL_OFFSET_Z=0
VOL_SIZE_X=3440
VOL_SIZE_Y=3340
VOL_SIZE_Z=11174
QUADMESH_SIZE=4
VOXEL_SIZE=7.91
RANDOM_SEED=123
VOLUME_ZARR="d:/zarrs/s4/20231117161658.zarr/0"
SURFACE_ZARR="d:/zarrs/s4_059_medial_ome.zarr/0"
VF_TEMP_ZARR="d:/vf_zarrs/s4/pvf_2025_11_17.zarr"
VECTORFIELD_ZARR="d:/vf_zarrs/s4/pvfs_2025_12_16.zarr"
# The initial seed point
SEED_X=1798
SEED_Y=1161
SEED_Z=5120
SEED_AXIS1_X=1
SEED_AXIS1_Y=0
SEED_AXIS1_Z=0
SEED_AXIS2_X=0
SEED_AXIS2_Y=0
SEED_AXIS2_Z=1
# Parameters used by the patch growing program
MIN_PATCH_ITERS=45
MAX_GROWTH_STEPS=225
SPRING_FORCE_CONSTANT=0.05
BEND_FORCE_CONSTANT=0.025
# This is select so as to make the system critically damped
FRICTION_CONSTANT=0.6
# VECTORFIELD_CONSTANT is ((0.02/125.0)/SPRING_FORCE_CONSTANT). Factor of 1/125.0f is inverse of max value in fector field zarr. Divide by spring force constant to make inner loop of Forces faster
VECTORFIELD_CONSTANT=0.0032
RELAX_FORCE_THRESHHOLD=0.015
MAX_RELAX_ITERATIONS=100
MIN_RELAX_ITERATIONS=15
HIGH_STRESS_THRESHHOLD=0.05
# Parameters related to testing patches and adding them to a surface
MAX_ROTATE_VARIANCE=0.1
MAX_TRANSLATE_VARIANCE=50
# If any points on the current boundary are within this distance of any points in the patch, erase them. Okay to be overzealous.
CURRENT_BOUNDARY_ERASE_DISTANCE=10
# If any points on the boundary to add are within this distance of the current surface, erase them
NEW_BOUNDARY_ERASE_DISTANCE=5
# Parameters related to the alignment algorithm (align_patchesN.cpp)
# The minimum distance between a point in one patch and another that will be considered before assessing it as a match
MATCH_MIN_DIST=2.0
# The number of point-pair samples that will be take per-patch when looking for transformations
NUM_POINT_SAMPLES=500
# The min length line that will be used when looking for transformations
MIN_LINE_LENGTH=50
# The maximum difference in line length tolerated when looking for transformations
MAX_LINE_DIFF=0.01
# Minimum number of transforms that must be found for transform and variance to be output
MIN_TRANSFORMS=25
