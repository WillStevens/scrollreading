/* Parameters shared by several programs */
#define OUTPUT_DIR d:/pipelineOutput
#define VOL_OFFSET_X 2176
#define VOL_OFFSET_Y 1536
#define VOL_OFFSET_Z 4608
#define VOL_SIZE_X 3072
#define VOL_SIZE_Y 3072
#define VOL_SIZE_Z 4096
#define QUADMESH_SIZE 4
#define VOXEL_SIZE 7.91
#define RANDOM_SEED 123
#define VOLUME_ZARR d:/zarrs/54keV_7.91um_Scroll1A.zarr/0/
#define VECTORFIELD_ZARR d:/pvf_2025_10_19.zarr
/* The initial seed point */
#define SEED_X 3700
#define SEED_Y 2408
#define SEED_Z 5632
#define SEED_AXIS1_X 1
#define SEED_AXIS1_Y 0
#define SEED_AXIS1_Z 0
#define SEED_AXIS2_X 0
#define SEED_AXIS2_Y 0
#define SEED_AXIS2_Z 1
/* Parameters used by the patch growing program */
#define MIN_PATCH_ITERS 45
#define MAX_GROWTH_STEPS 225
#define SPRING_FORCE_CONSTANT 0.05
#define BEND_FORCE_CONSTANT 0.025
/* This is select so as to make the system critically damped */
#define FRICTION_CONSTANT 0.6
/* Factor of 1/73.0f was added when changing to single-byte vector field. Divide by spring force constant to make inner loop of Forces faster */
#define VECTORFIELD_CONSTANT ((0.05/73.0)/SPRING_FORCE_CONSTANT)
#define RELAX_FORCE_THRESHHOLD 0.01
#define MAX_RELAX_ITERATIONS 100
/* Parameters related to testing patches and adding them to a surface */
#define MAX_ROTATE_VARIANCE 0.1
#define MAX_TRANSLATE_VARIANCE 10
/* If any points on the current boundary are within this distance of any points in the patch, erase them. Okay to be overzealous. */
#define CURRENT_BOUNDARY_ERASE_DISTANCE 10
/* If any points on the boundary to add are within this distance of the current surface, erase them */
#define NEW_BOUNDARY_ERASE_DISTANCE 5
/* Parameters related to the alignment algorithm (align_patchesN.cpp) */
/* The minimum distance between a point in one patch and another that will be considered before assessing it as a match */
#define MATCH_MIN_DIST 2.0
/* The number of point-pair samples that will be take per-patch when looking for transformations */
#define NUM_POINT_SAMPLES 500
/* The min length line that will be used when looking for transformations */
#define MIN_LINE_LENGTH 50
/* The maximum difference in line length tolerated when looking for transformations */
#define MAX_LINE_DIFF 0.01
/* Minimum number of transforms that must be found for transform and variance to be output */
#define MIN_TRANSFORMS 25
