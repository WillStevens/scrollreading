// tifxyz_to_points.cpp  (libtiff-only, no OpenCV dependency)
//
// The reverse of patch_to_tifxyz.cpp: reads a VC3D tifxyz surface
// (x.tif, y.tif, z.tif + meta.json) and emits one line per valid grid
// cell: "ux,uy,vx,vy,vz", where:
//   - vx,vy,vz are the 3D coordinates stored in that cell of x/y/z.tif
//   - ux,uy are the 2D "nominal" surface coordinates recovered from the
//     cell's (row,col) position and meta.json's "scale" field:
//         ux = col / scale_x
//         uy = row / scale_y
//     (this mirrors QuadSurface's own nominal-coordinate convention --
//     see internal_loc()/nominal_loc() and the "size = points / scale"
//     relationship in QuadSurface.cpp)
//
// A cell is considered invalid/missing -- and skipped -- if z <= 0, which
// is exactly the rule VC3D's own loader uses (QuadSurface.cpp,
// "// Invalidate by z<=0"), regardless of what x/y happen to contain.
//
// TIFF reading mirrors VC3D's read_band_into() (QuadSurface.cpp): it
// accepts 1-sample-per-pixel images at 8/16/32/64 bits, in UINT, INT, or
// IEEE float sample format, and handles both scanline (stripped) and
// tiled TIFFs. In practice x/y/z.tif written by VC3D (and by
// patch_to_tifxyz.cpp) are always untiled 32-bit float, but supporting
// the same range the reader does means this will also work on tifxyz
// data produced or re-saved by other tools.
//
// Build (needs only libtiff):
//   g++ -std=c++17 -O2 tifxyz_to_points.cpp -o tifxyz_to_points -ltiff
//
// Usage:
//   ./tifxyz_to_points <tifxyz_dir> [output.csv]
//
// If output.csv is omitted, points are written to stdout. A header line
// "ux,uy,vx,vy,vz" is written first.

#include <tiffio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// A tiny row-major single-channel float image, mirroring the one used in
// patch_to_tifxyz.cpp.
struct FloatImage {
    int width = 0;
    int height = 0;
    std::vector<float> data; // size = width*height, row-major

    FloatImage() = default;
    FloatImage(int w, int h, float fill)
        : width(w), height(h), data(static_cast<size_t>(w) * h, fill) {}

    float &at(int row, int col) { return data[static_cast<size_t>(row) * width + col]; }
    const float &at(int row, int col) const { return data[static_cast<size_t>(row) * width + col]; }
};

// ---------------------------------------------------------------------
// meta.json: we only need the "scale" field, so a tiny hand-rolled
// extractor is enough and avoids a JSON library dependency.
// ---------------------------------------------------------------------

double read_scale_from_meta(const fs::path &metaPath, double &scaleX, double &scaleY)
{
    std::ifstream in(metaPath);
    if (!in) {
        throw std::runtime_error("failed to open " + metaPath.string());
    }
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string content = buf.str();

    // Matches:  "scale": [ 1.5, 2 ]   (whitespace-tolerant, handles ints/floats/exponents)
    static const std::regex scaleRe(
        R"("scale"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])");
    std::smatch m;
    if (!std::regex_search(content, m, scaleRe)) {
        throw std::runtime_error("could not find \"scale\" field in " + metaPath.string());
    }
    scaleX = std::stod(m[1].str());
    scaleY = std::stod(m[2].str());
    return scaleX;
}

// ---------------------------------------------------------------------
// TIFF reading -- mirrors VC3D's read_band_into() closely enough to
// handle the same range of inputs.
// ---------------------------------------------------------------------

// Converts a raw sample (bytes at `p`) to float given its TIFF sample
// format/bit depth, same rules as QuadSurface.cpp's to_float lambda.
float sample_to_float(const uint8_t *p, uint16_t fmt, uint16_t bps, int bytesPer)
{
    switch (fmt) {
        case SAMPLEFORMAT_IEEEFP:
            if (bps == 32) { float v; std::memcpy(&v, p, 4); return v; }
            if (bps == 64) { double d; std::memcpy(&d, p, 8); return static_cast<float>(d); }
            break;
        case SAMPLEFORMAT_UINT:
            if (bps == 8)  { uint8_t  t = *p; return static_cast<float>(t); }
            if (bps == 16) { uint16_t t; std::memcpy(&t, p, 2); return static_cast<float>(t); }
            if (bps == 32) { uint32_t t; std::memcpy(&t, p, 4); return static_cast<float>(t); }
            break;
        case SAMPLEFORMAT_INT:
            if (bps == 8)  { int8_t  t = *reinterpret_cast<const int8_t *>(p); return static_cast<float>(t); }
            if (bps == 16) { int16_t t; std::memcpy(&t, p, 2); return static_cast<float>(t); }
            if (bps == 32) { int32_t t; std::memcpy(&t, p, 4); return static_cast<float>(t); }
            break;
        default:
            break;
    }
    // Last resort: treat as 32-bit float bytes, same fallback as VC3D.
    float v = 0.f;
    std::memcpy(&v, p, std::min(bytesPer, static_cast<int>(sizeof(float))));
    return v;
}

// Reads a single-channel TIFF into `out` (allocated to the file's own
// width/height on first call). `channel` selects which of x/y/z this
// file's data represents, matching a shared (width,height) across all
// three bands.
void read_band_into(const fs::path &path, FloatImage bands[3], int channel, int &W, int &H)
{
    TIFF *tif = TIFFOpen(path.string().c_str(), "r");
    if (!tif) {
        throw std::runtime_error("failed to open TIFF: " + path.string());
    }

    uint32_t w = 0, h = 0;
    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w) || !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h)) {
        TIFFClose(tif);
        throw std::runtime_error("TIFF missing width/height: " + path.string());
    }

    uint16_t spp = 1;
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    if (spp != 1) {
        TIFFClose(tif);
        throw std::runtime_error("expected 1 sample per pixel in " + path.string());
    }
    uint16_t bps = 0;
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bps);
    uint16_t fmt = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &fmt);
    const int bytesPer = (bps + 7) / 8;
    if (!(bps == 8 || bps == 16 || bps == 32 || bps == 64)) {
        TIFFClose(tif);
        throw std::runtime_error("unsupported BitsPerSample in " + path.string());
    }

    if (bands[0].width == 0 && bands[0].height == 0 &&
        bands[1].width == 0 && bands[1].height == 0 &&
        bands[2].width == 0 && bands[2].height == 0) {
        // First band read: allocate all three to this file's dimensions,
        // pre-filled as invalid (-1), same as QuadSurface's loader.
        for (int i = 0; i < 3; ++i) {
            bands[i] = FloatImage(static_cast<int>(w), static_cast<int>(h), -1.0f);
        }
        W = static_cast<int>(w);
        H = static_cast<int>(h);
    } else if (static_cast<int>(w) != W || static_cast<int>(h) != H) {
        TIFFClose(tif);
        throw std::runtime_error("x/y/z.tif dimensions do not match in " + path.string());
    }

    FloatImage &dst = bands[channel];

    if (TIFFIsTiled(tif)) {
        uint32_t tileW = 0, tileH = 0;
        TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileW);
        TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileH);
        if (tileW == 0 || tileH == 0) {
            TIFFClose(tif);
            throw std::runtime_error("invalid tile geometry in " + path.string());
        }
        const tmsize_t tileBytes = TIFFTileSize(tif);
        std::vector<uint8_t> tileBuf(static_cast<size_t>(tileBytes));

        for (uint32_t y0 = 0; y0 < h; y0 += tileH) {
            const uint32_t copyY1 = std::min(y0 + tileH, h);
            for (uint32_t x0 = 0; x0 < w; x0 += tileW) {
                const uint32_t copyX1 = std::min(x0 + tileW, w);
                const ttile_t tidx = TIFFComputeTile(tif, x0, y0, 0, 0);
                tmsize_t n = TIFFReadEncodedTile(tif, tidx, tileBuf.data(), tileBytes);
                if (n < 0) {
                    std::fill(tileBuf.begin(), tileBuf.end(), 0);
                }
                for (uint32_t y = y0; y < copyY1; ++y) {
                    const uint32_t ty = y - y0;
                    const uint8_t *row = tileBuf.data() + static_cast<size_t>(ty) * tileW * bytesPer;
                    for (uint32_t x = x0; x < copyX1; ++x) {
                        const uint32_t tx = x - x0;
                        dst.at(static_cast<int>(y), static_cast<int>(x)) =
                            sample_to_float(row + static_cast<size_t>(tx) * bytesPer, fmt, bps, bytesPer);
                    }
                }
            }
        }
    } else {
        const tmsize_t scanBytes = TIFFScanlineSize(tif);
        std::vector<uint8_t> scanBuf(static_cast<size_t>(scanBytes));
        for (uint32_t y = 0; y < h; ++y) {
            if (TIFFReadScanline(tif, scanBuf.data(), y, 0) != 1) {
                std::fill(scanBuf.begin(), scanBuf.end(), 0);
            }
            const uint8_t *row = scanBuf.data();
            for (uint32_t x = 0; x < w; ++x) {
                dst.at(static_cast<int>(y), static_cast<int>(x)) =
                    sample_to_float(row + static_cast<size_t>(x) * bytesPer, fmt, bps, bytesPer);
            }
        }
    }

    TIFFClose(tif);
}

// ---------------------------------------------------------------------
// Core conversion
// ---------------------------------------------------------------------

struct Point {
    double ux, uy, vx, vy, vz;
};

std::vector<Point> read_tifxyz(const fs::path &dir)
{
    double scaleX = 1.0, scaleY = 1.0;
    read_scale_from_meta(dir / "meta.json", scaleX, scaleY);
    if (scaleX == 0.0 || scaleY == 0.0) {
        throw std::runtime_error("meta.json \"scale\" contains a zero component");
    }

    FloatImage bands[3]; // 0=x, 1=y, 2=z
    int W = 0, H = 0;
    read_band_into(dir / "x.tif", bands, 0, W, H);
    read_band_into(dir / "y.tif", bands, 1, W, H);
    read_band_into(dir / "z.tif", bands, 2, W, H);

    std::vector<Point> points;
    points.reserve(static_cast<size_t>(W) * H);
    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            const float vx = bands[0].at(row, col);
            const float vy = bands[1].at(row, col);
            const float vz = bands[2].at(row, col);
            if (vz <= 0.f) {
                continue; // invalid/missing cell, same rule VC3D uses
            }
            const double ux = col / scaleX;
            const double uy = row / scaleY;
            points.push_back({ux, uy, static_cast<double>(vx), static_cast<double>(vy), static_cast<double>(vz)});
        }
    }
    return points;
}

// ---------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------

void print_usage(const char *argv0)
{
    std::cerr << "usage: " << argv0 << " <tifxyz_dir> [output.csv]\n"
              << "  writes ux,uy,vx,vy,vz to output.csv, or stdout if omitted\n";
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const fs::path dir = argv[1];

    try {
        const std::vector<Point> points = read_tifxyz(dir);
        std::cerr << "read " << points.size() << " valid point(s) from " << dir << "\n";

        std::ostream *out = &std::cout;
        std::ofstream fileOut;
        if (argc >= 3) {
            fileOut.open(argv[2]);
            if (!fileOut) {
                throw std::runtime_error(std::string("failed to open output file: ") + argv[2]);
            }
            out = &fileOut;
        }

        *out << "ux,uy,vx,vy,vz\n";
        out->precision(9);
        for (const auto &p : points) {
            *out << p.ux << "," << p.uy << "," << p.vx << "," << p.vy << "," << p.vz << "\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}