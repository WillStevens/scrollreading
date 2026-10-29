// gridsplice.cpp
//
// Usage:
//   gridsplice <target>.bin x,y <to-splice>.bin <output>.bin
//
// The patch's 2D bounding rectangle has its lower-left corner (uxmin,uymin)
// moved to (x,y); every patch point's (ux,uy) is translated by the same
// amount. The 3D coordinates (vx,vy,vz) of the patch are left untouched.
//
// Every target point whose (ux,uy) lies inside the translated patch rectangle
// (bounds inclusive) is erased, and the patch points are inserted in its
// place: the patch is written at the position of the first erased target point
// (or appended at the end if nothing was erased). All other target points keep
// their original order.
//
// Build:  g++ -O2 -std=c++11 -o gridsplice gridsplice.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

typedef struct __attribute__((packed)) {
    float ux, uy, vx, vy, vz;
} gridPointStruct;

static_assert(sizeof(gridPointStruct) == 5 * sizeof(float), "struct must be packed");

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <target>.bin x,y <to-splice>.bin <output>.bin\n", prog);
}

static bool parseXY(const char *s, float &x, float &y)
{
    int consumed = 0;
    if (sscanf(s, "%f,%f%n", &x, &y, &consumed) != 2) return false;
    return s[consumed] == '\0';
}

// Reads a whole file of gridPointStructs into memory.
static bool readAll(const char *path, std::vector<gridPointStruct> &pts)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }

    const size_t CHUNK = 1 << 16;
    std::vector<gridPointStruct> buf(CHUNK);
    size_t n;
    while ((n = fread(buf.data(), sizeof(gridPointStruct), CHUNK, f)) > 0)
        pts.insert(pts.end(), buf.begin(), buf.begin() + n);

    bool ok = !ferror(f);
    if (!ok) perror(path);
    else {
        long pos = ftell(f);
        if (pos >= 0 && (size_t)pos != pts.size() * sizeof(gridPointStruct))
            fprintf(stderr, "Warning: %s size is not a multiple of %zu bytes; trailing partial record ignored\n",
                    path, sizeof(gridPointStruct));
    }
    fclose(f);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 5) { usage(argv[0]); return 1; }

    const char *targetPath = argv[1];
    const char *patchPath  = argv[3];
    const char *outPath    = argv[4];

    float x, y;
    if (!parseXY(argv[2], x, y)) {
        fprintf(stderr, "Error: could not parse position '%s' (expected x,y)\n", argv[2]);
        return 1;
    }
    if (!strcmp(outPath, targetPath) || !strcmp(outPath, patchPath)) {
        fprintf(stderr, "Error: output file must differ from the input files\n");
        return 1;
    }

    // ---- load the patch ----
    std::vector<gridPointStruct> patch;
    if (!readAll(patchPath, patch)) return 1;
    if (patch.empty()) {
        fprintf(stderr, "Error: patch file contains no points\n");
        return 1;
    }

    float pxmin = INFINITY, pymin = INFINITY;
    for (const gridPointStruct &p : patch) {
        if (std::isnan(p.ux) || std::isnan(p.uy)) continue;
        if (p.ux < pxmin) pxmin = p.ux;
        if (p.uy < pymin) pymin = p.uy;
    }
    if (std::isinf(pxmin)) {
        fprintf(stderr, "Error: patch has no valid 2D coordinates\n");
        return 1;
    }

    // ---- translate the patch and compute its footprint ----
    const float dx = x - pxmin, dy = y - pymin;
    float bxmin = INFINITY, bymin = INFINITY, bxmax = -INFINITY, bymax = -INFINITY;
    for (gridPointStruct &p : patch) {
        p.ux += dx;
        p.uy += dy;
        if (std::isnan(p.ux) || std::isnan(p.uy)) continue;
        if (p.ux < bxmin) bxmin = p.ux;
        if (p.ux > bxmax) bxmax = p.ux;
        if (p.uy < bymin) bymin = p.uy;
        if (p.uy > bymax) bymax = p.uy;
    }

    // ---- stream the target into the output ----
    FILE *in = fopen(targetPath, "rb");
    if (!in) { perror(targetPath); return 1; }
    FILE *out = fopen(outPath, "wb");
    if (!out) { perror(outPath); fclose(in); return 1; }

    const size_t CHUNK = 1 << 16;
    std::vector<gridPointStruct> buf(CHUNK), kept;
    kept.reserve(CHUNK);

    size_t total = 0, erased = 0, written = 0;
    bool patchWritten = false;
    int rc = 0;

    auto emit = [&](const std::vector<gridPointStruct> &v) -> bool {
        if (v.empty()) return true;
        if (fwrite(v.data(), sizeof(gridPointStruct), v.size(), out) != v.size()) {
            perror("write");
            return false;
        }
        written += v.size();
        return true;
    };

    size_t n;
    while (rc == 0 && (n = fread(buf.data(), sizeof(gridPointStruct), CHUNK, in)) > 0) {
        total += n;
        kept.clear();
        for (size_t i = 0; i < n; ++i) {
            const gridPointStruct &p = buf[i];
            const bool inside = p.ux >= bxmin && p.ux <= bxmax &&
                                p.uy >= bymin && p.uy <= bymax;   // false for NaN
            if (inside) {
                ++erased;
                if (!patchWritten) {
                    // flush what precedes this point, then drop the patch in
                    if (!emit(kept) || !emit(patch)) { rc = 1; break; }
                    kept.clear();
                    patchWritten = true;
                }
            } else {
                kept.push_back(p);
            }
        }
        if (rc == 0 && !emit(kept)) rc = 1;
    }

    if (rc == 0 && ferror(in)) { perror("read"); rc = 1; }
    if (rc == 0 && !patchWritten) {
        if (!emit(patch)) rc = 1;     // nothing overlapped: append
    }

    fclose(in);
    if (fclose(out) != 0) { perror("close output"); rc = 1; }
    if (rc != 0) return rc;

    fprintf(stderr,
            "Target: %zu points, erased %zu inside [%g,%g]-[%g,%g]; inserted %zu patch points; wrote %zu points\n",
            total, erased, bxmin, bymin, bxmax, bymax, patch.size(), written);
    return 0;
}