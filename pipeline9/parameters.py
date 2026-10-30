# Parameters shared by several programs
OUTPUT_DIR="c:/vesuvius/output/PHerc1667"
VOL_OFFSET_X=0
VOL_OFFSET_Y=0
VOL_OFFSET_Z=0
VOL_SIZE_X=3440
VOL_SIZE_Y=3340
VOL_SIZE_Z=11174
QUADMESH_SIZE=4
VOXEL_SIZE=7.91
RANDOM_SEED=124
VOLUME_ZARR="c:/vesuvius/scroll_data/zarrs/PHerc1667/volume/2"
SURFACE_ZARR="c:/vesuvius/scroll_data/zarrs/PHerc1667/surface/0"
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
MAX_GROWTH_STEPS=125
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
MAX_ROTATE_VARIANCE=0.02
MAX_TRANSLATE_VARIANCE=5
# If any points on the current boundary are within this distance of any points in the patch, erase them. Okay to be overzealous.
CURRENT_BOUNDARY_ERASE_DISTANCE=10
# If any points on the boundary to add are within this distance of the current surface, erase them
NEW_BOUNDARY_ERASE_DISTANCE=5
# Parameters related to the alignment algorithm (align_patchesN.cpp)
# The minimum distance between a point in one patch and another that will be considered before assessing it as a match
MATCH_MIN_DIST=8.0
# The number of point-pair samples that will be take per-patch when looking for transformations
NUM_POINT_SAMPLES=1000
# The min length line that will be used when looking for transformations
MIN_LINE_LENGTH=50
# The maximum difference in line length tolerated when looking for transformations
MAX_LINE_DIFF=0.01
# Minimum number of transforms that must be found for transform and variance to be output
MIN_TRANSFORMS=25
# Max volume distance allowed for overlapping x,y points
BP_MAX_XYZ_DISTANCE=10
# How many quadmesh steps to take when scoring a surface (e.g. SCOREPLACEMENT_SCALE = 10 and QUADMESH_SIZE = 4 means 40 voxel spaces per step)
SCOREPLACEMENT_SCALE=5
# Weight given to score returned by PatchSpringSimulaiton (total partch movement). If changing SCOREPLACEMENT_SCALE by a factor N, then STRETCH_SCORE_FACTOR should be divided by a factor N^2 to keep it in-sync. Values tried for both have been 5,1 and 10,0.25
STRETCH_SCORE_FACTOR=1.0
# At the time of writing this (2026-10-28) it is beleived that the current implementation of patch heat doesn't help. Patches that manifest inconsistencies are more likely to be symptoms of a problem than the causes, so giving them extra weight prevents the causes from being visited by MutateState
# exponential decay applied each iteration
HEAT_DECAY=0.85
# added per appearance in patchesInvolved
HEAT_INCREMENT=1.0
# floor weight so cold/unseen patches still get proposed
EXPLORATION_EPSILON=0.2
