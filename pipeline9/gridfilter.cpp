// gridfilter.cpp
//
// Usage:
//   gridfilter <input>.bin
//       Prints the min and max 2D (ux,uy) coordinates found in the file.
//
//   gridfilter <input>.bin uxmin,uymin,uxmax,uymax <output>.bin
//       Writes only the points whose (ux,uy) lie inside the rectangle
//       (bounds inclusive) to the output file.
//
// Build:  g++ -O2 -std=c++11 -o gridfilter gridfilter.cpp

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
    fprintf(stderr,
            "Usage:\n"
            "  %s <input>.bin\n"
            "      print min/max of the 2D coordinates\n"
            "  %s <input>.bin uxmin,uymin,uxmax,uymax <output>.bin\n"
            "      write only points inside the bounding rectangle\n",
            prog, prog);
}

// Parses "a,b,c,d" into four floats. Returns true on success.
static bool parseBounds(const char *s, float &uxmin, float &uymin, float &uxmax, float &uymax)
{
    int consumed = 0;
    if (sscanf(s, "%f,%f,%f,%f%n", &uxmin, &uymin, &uxmax, &uymax, &consumed) != 4)
        return false;
    return s[consumed] == '\0';   // reject trailing garbage
}

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 4) {
        usage(argv[0]);
        return 1;
    }

    const bool filterMode = (argc == 4);
    float bxmin = 0, bymin = 0, bxmax = 0, bymax = 0;

    if (filterMode) {
        if (!parseBounds(argv[2], bxmin, bymin, bxmax, bymax)) {
            fprintf(stderr, "Error: could not parse bounds '%s' (expected uxmin,uymin,uxmax,uymax)\n", argv[2]);
            return 1;
        }
        if (bxmin > bxmax || bymin > bymax) {
            fprintf(stderr, "Error: bounds are inverted (min > max)\n");
            return 1;
        }
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        perror(argv[1]);
        return 1;
    }

    FILE *out = nullptr;
    if (filterMode) {
        out = fopen(argv[3], "wb");
        if (!out) {
            perror(argv[3]);
            fclose(in);
            return 1;
        }
    }

    const size_t CHUNK = 1 << 16;                 // points per read
    std::vector<gridPointStruct> buf(CHUNK);
    std::vector<gridPointStruct> kept;
    if (filterMode) kept.reserve(CHUNK);

    size_t total = 0, written = 0;
    float uxmin = INFINITY, uymin = INFINITY, uxmax = -INFINITY, uymax = -INFINITY;
    int rc = 0;

    size_t n;
    while ((n = fread(buf.data(), sizeof(gridPointStruct), CHUNK, in)) > 0) {
        total += n;

        if (filterMode) {
            kept.clear();
            for (size_t i = 0; i < n; ++i) {
                const gridPointStruct &p = buf[i];
                // NaN coordinates fail these comparisons and are dropped.
                if (p.ux >= bxmin && p.ux <= bxmax &&
                    p.uy >= bymin && p.uy <= bymax)
                    kept.push_back(p);
            }
            if (!kept.empty()) {
                if (fwrite(kept.data(), sizeof(gridPointStruct), kept.size(), out) != kept.size()) {
                    perror("write");
                    rc = 1;
                    break;
                }
                written += kept.size();
            }
        } else {
            for (size_t i = 0; i < n; ++i) {
                const gridPointStruct &p = buf[i];
                if (std::isnan(p.ux) || std::isnan(p.uy)) continue;
                if (p.ux < uxmin) uxmin = p.ux;
                if (p.ux > uxmax) uxmax = p.ux;
                if (p.uy < uymin) uymin = p.uy;
                if (p.uy > uymax) uymax = p.uy;
            }
        }
    }

    if (ferror(in)) {
        perror("read");
        rc = 1;
    }

    // Detect a truncated trailing record (file size not a multiple of 20 bytes).
    if (rc == 0 && feof(in)) {
        long pos = ftell(in);
        if (pos >= 0 && (size_t)pos != total * sizeof(gridPointStruct))
            fprintf(stderr, "Warning: file size is not a multiple of %zu bytes; trailing partial record ignored\n",
                    sizeof(gridPointStruct));
    }

    fclose(in);
    if (out && fclose(out) != 0) {
        perror("close output");
        rc = 1;
    }

    if (rc != 0) return rc;

    if (filterMode) {
        fprintf(stderr, "Read %zu points, wrote %zu points\n", total, written);
    } else if (total == 0) {
        fprintf(stderr, "File contains no points\n");
    } else {
        printf("Points: %zu\n", total);
        printf("min: %.9g, %.9g\n", uxmin, uymin);
        printf("max: %.9g, %.9g\n", uxmax, uymax);
    }
    return 0;
}