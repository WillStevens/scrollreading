// seed_runtime.cpp: the per seed values of parameters.h read when simpaper10 starts, so one binary serves every seed.
//
// parameters.h compiles in the seed point and its two axes (SEED_X ... SEED_AXIS2_Z) and the surface zarr path
// (SURFACE_ZARR). They are the defaults here. At start, before main, this unit reads:
//   SIMPAPER_SEED_X, SIMPAPER_SEED_Y, SIMPAPER_SEED_Z            all three or none
//   SIMPAPER_SEED_AXIS1_X ... SIMPAPER_SEED_AXIS2_Z              all six or none
//   SIMPAPER_SURFACE_ZARR                                        the path modes A, p and s open (growth already reads it)
// A value must be a whole number string (strtod consumes it all, finite); a partial group or a bad value exits 2.
// One line on stderr says which values are in effect and where they came from.
//
// The values enter simpaper10.cpp's float seed array exactly as the compiled constants did: an integer converted to
// float is the same float whether it was a literal or parsed, for every integer below 2^24. The globals are read with
// loads, not calls, so no function of simpaper10.cpp gains a call or a branch.
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "parameters.h"

float sa_seed_runtime[9] = {(float)(SEED_X), (float)(SEED_Y), (float)(SEED_Z),
                            (float)(SEED_AXIS1_X), (float)(SEED_AXIS1_Y), (float)(SEED_AXIS1_Z),
                            (float)(SEED_AXIS2_X), (float)(SEED_AXIS2_Y), (float)(SEED_AXIS2_Z)};
const char *sa_surface_zarr = SURFACE_ZARR;

static const char *const sa_seed_env[9] = {
    "SIMPAPER_SEED_X", "SIMPAPER_SEED_Y", "SIMPAPER_SEED_Z",
    "SIMPAPER_SEED_AXIS1_X", "SIMPAPER_SEED_AXIS1_Y", "SIMPAPER_SEED_AXIS1_Z",
    "SIMPAPER_SEED_AXIS2_X", "SIMPAPER_SEED_AXIS2_Y", "SIMPAPER_SEED_AXIS2_Z"};

static int sa_read_group(int from, int n)
{
    int set = 0;
    for (int i = from; i < from + n; i++)
    {
        const char *e = getenv(sa_seed_env[i]);
        if (e && *e) set++;
    }
    if (set == 0) return 0;
    if (set != n)
    {
        fprintf(stderr, "seed at run time: %d of the %d variables %s ... %s are set; set all or none\n",
                set, n, sa_seed_env[from], sa_seed_env[from + n - 1]);
        exit(2);
    }
    for (int i = from; i < from + n; i++)
    {
        const char *e = getenv(sa_seed_env[i]);
        char *end = 0;
        errno = 0;
        double v = strtod(e, &end);
        if (errno != 0 || end == e || *end != '\0' || !std::isfinite(v))
        {
            fprintf(stderr, "seed at run time: %s=[%s] is not a number\n", sa_seed_env[i], e);
            exit(2);
        }
        sa_seed_runtime[i] = (float)v;
    }
    return 1;
}

__attribute__((constructor)) static void sa_seed_runtime_read(void)
{
    int point = sa_read_group(0, 3);
    int axes = sa_read_group(3, 6);
    const char *sz = getenv("SIMPAPER_SURFACE_ZARR");
    if (sz && *sz) sa_surface_zarr = sz;
    fprintf(stderr, "Seed at run time: %.9g %.9g %.9g (%s), axes %.9g %.9g %.9g %.9g %.9g %.9g (%s), surface zarr %s (%s)\n",
            sa_seed_runtime[0], sa_seed_runtime[1], sa_seed_runtime[2], point ? "environment" : "compiled",
            sa_seed_runtime[3], sa_seed_runtime[4], sa_seed_runtime[5],
            sa_seed_runtime[6], sa_seed_runtime[7], sa_seed_runtime[8], axes ? "environment" : "compiled",
            sa_surface_zarr, (sz && *sz) ? "environment" : "compiled");
    fflush(stderr);
}
