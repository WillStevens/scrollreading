// patch_to_tifxyz.cpp  (libtiff-only version, no OpenCV dependency)
//
// Converts a list of points (ux, uy, vx, vy, vz) -- 2D surface ("nominal")
// coordinates plus their corresponding 3D volume coordinates -- into the
// tifxyz format read/written by VC3D (ScrollPrize/villa, QuadSurface.cpp):
//
//   <out_dir>/x.tif     single-channel, 32-bit float, uncompressed TIFF
//   <out_dir>/y.tif     single-channel, 32-bit float, uncompressed TIFF
//   <out_dir>/z.tif     single-channel, 32-bit float, uncompressed TIFF
//   <out_dir>/meta.json {"format":"tifxyz","type":"seg","uuid":...,
//                         "bbox":[[xlo,ylo,zlo],[xhi,yhi,zhi]],
//                         "scale":[sx,sy]}
//
// Cells with no point mapped to them are written as (-1,-1,-1), which is
// what VC3D treats as "missing" (its loader invalidates any cell with
// z <= 0 regardless of the exact value).
//
// The TIFF writing below mirrors VC3D's own writeTiff() in
// volume-cartographer/core/src/Tiff.cpp (untiled/scanline path) tag for
// tag: 1 sample/pixel, 32 bits/sample, IEEE float, MINISBLACK, no
// compression, one strip covering the whole image. VC3D's loader
// (QuadSurface.cpp's read_band_into) is in fact tolerant of other
// bit depths/compression too, but matching the writer exactly is the
// safest bet for round-tripping.
//
// Build (needs only libtiff):
//   g++ -std=c++17 -O2 patch_to_tifxyz.cpp -o patch_to_tifxyz -ltiff
//
// Usage:
//   ./patch_to_tifxyz points.bin out_dir/ [--uuid STRING] [--step SX SY] [--no-overwrite]
//
// points.bin: binary file where each point is 5 floats "ux,uy,vx,vy,vz"
//
// You can also just call write_tifxyz() directly from your own code
// instead of using the CLI -- that's probably the more useful entry point
// if this is part of a bigger pipeline that already has the points in
// memory.

#include <tiffio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

typedef struct __attribute__((packed)) {float x,y,px,py,pz;} gridPointStruct;

struct Point {
    double ux = 0.0, uy = 0.0;           // 2D surface ("nominal") coordinates
    double vx = 0.0, vy = 0.0, vz = 0.0; // 3D volume coordinates
};

// A tiny row-major single-channel float image, just enough to avoid
// depending on OpenCV for this.
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
// Small helpers
// ---------------------------------------------------------------------

std::string make_uuid_v4()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> hex(0, 15);
    const char *digits = "0123456789abcdef";
    auto nibble = [&]() { return digits[hex(gen)]; };

    std::string uuid;
    const std::string layout = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    uuid.reserve(layout.size());
    for (char c : layout) {
        if (c == 'x') {
            uuid.push_back(nibble());
        } else if (c == 'y') {
            static const char variant[] = {'8', '9', 'a', 'b'}; // RFC 4122 variant bits
            uuid.push_back(variant[hex(gen) % 4]);
        } else {
            uuid.push_back(c);
        }
    }
    return uuid;
}

// Writes a single-channel float32 image as an untiled, uncompressed TIFF,
// matching VC3D's writeTiff(..., tileW=0, tileH=0, ..., COMPRESSION_NONE, ...)
// scanline path (see volume-cartographer/core/src/Tiff.cpp).
void write_float_tiff(const fs::path &path, const FloatImage &img)
{
    const uint32_t W = static_cast<uint32_t>(img.width);
    const uint32_t H = static_cast<uint32_t>(img.height);

    // Match VC3D's choice of classic vs. BigTIFF based on raw pixel size.
    const std::uint64_t pixelBytes = static_cast<std::uint64_t>(W) * H * sizeof(float);
    const char *mode = pixelBytes > 0xffff0000ULL ? "w8" : "w";

    TIFF *tf = TIFFOpen(path.string().c_str(), mode);
    if (!tf) {
        throw std::runtime_error("Failed to open TIFF for writing: " + path.string());
    }

    TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, W);
    TIFFSetField(tf, TIFFTAG_IMAGELENGTH, H);
    TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 32);
    TIFFSetField(tf, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
    TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tf, TIFFTAG_ROWSPERSTRIP, H); // whole image as one strip

    for (uint32_t y = 0; y < H; ++y) {
        const uint8_t *row = reinterpret_cast<const uint8_t *>(img.data.data() + static_cast<size_t>(y) * W);
        if (TIFFWriteScanline(tf, const_cast<uint8_t *>(row), y, 0) < 0) {
            TIFFClose(tf);
            throw std::runtime_error("TIFFWriteScanline failed at row " + std::to_string(y) +
                                      " in " + path.string());
        }
    }

    if (!TIFFWriteDirectory(tf)) {
        TIFFClose(tf);
        throw std::runtime_error("TIFFWriteDirectory failed for " + path.string());
    }
    TIFFClose(tf);
}

// ---------------------------------------------------------------------
// Core conversion
// ---------------------------------------------------------------------

struct TifxyzOptions {
    // Spacing between adjacent grid samples in (ux,uy) units. Use {1,1}
    // (the default) if ux,uy are already integer-ish pixel/grid indices.
    // If your ux,uy are in some other unit (e.g. physical distance along
    // the flattened surface) and you want to resample onto a regular grid
    // at a given resolution, set this to that resolution instead.
    double step_x = 1.0;
    double step_y = 1.0;
    std::string uuid;         // left empty -> a random UUID v4 is generated
    bool force_overwrite = true;
};

// Converts `points` into tifxyz files under `out_dir`. Returns the uuid
// actually written to meta.json (useful if you let one be auto-generated).
std::string write_tifxyz(const std::vector<Point> &points,
                          const fs::path &out_dir,
                          TifxyzOptions opts = {})
{
    if (points.empty()) {
        throw std::runtime_error("write_tifxyz: no points given");
    }
    if (opts.step_x <= 0.0 || opts.step_y <= 0.0) {
        throw std::runtime_error("write_tifxyz: step must be positive");
    }

    // --- 1. Determine the (ux,uy) grid extent ---------------------------
    double umin = std::numeric_limits<double>::infinity();
    double umax = -std::numeric_limits<double>::infinity();
    double vmin = std::numeric_limits<double>::infinity();
    double vmax = -std::numeric_limits<double>::infinity();
    for (const auto &p : points) {
        umin = std::min(umin, p.ux);
        umax = std::max(umax, p.ux);
        vmin = std::min(vmin, p.uy);
        vmax = std::max(vmax, p.uy);
    }

    const int width  = static_cast<int>(std::llround((umax - umin) / opts.step_x)) + 1;
    const int height = static_cast<int>(std::llround((vmax - vmin) / opts.step_y)) + 1;
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("write_tifxyz: computed non-positive grid size");
    }

    // --- 2. Rasterize points onto the grid, tracking the 3D bbox --------
    FloatImage xImg(width, height, -1.0f);
    FloatImage yImg(width, height, -1.0f);
    FloatImage zImg(width, height, -1.0f);

    double bboxLo[3] = {std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity()};
    double bboxHi[3] = {-std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()};

    size_t dropped = 0;
    for (const auto &p : points) {
        const int col = static_cast<int>(std::llround((p.ux - umin) / opts.step_x));
        const int row = static_cast<int>(std::llround((p.uy - vmin) / opts.step_y));
        if (col < 0 || col >= width || row < 0 || row >= height) {
            ++dropped; // shouldn't happen given how width/height were derived
            continue;
        }
        xImg.at(row, col) = static_cast<float>(p.vx);
        yImg.at(row, col) = static_cast<float>(p.vy);
        zImg.at(row, col) = static_cast<float>(p.vz);

        bboxLo[0] = std::min(bboxLo[0], p.vx); bboxHi[0] = std::max(bboxHi[0], p.vx);
        bboxLo[1] = std::min(bboxLo[1], p.vy); bboxHi[1] = std::max(bboxHi[1], p.vy);
        bboxLo[2] = std::min(bboxLo[2], p.vz); bboxHi[2] = std::max(bboxHi[2], p.vz);
    }
    if (dropped > 0) {
        std::cerr << "write_tifxyz: warning: " << dropped
                  << " point(s) fell outside the computed grid and were skipped\n";
    }

    // --- 3. Create output dir --------------------------------------------
    if (fs::exists(out_dir)) {
        if (!opts.force_overwrite) {
            throw std::runtime_error("write_tifxyz: " + out_dir.string() + " already exists");
        }
    } else {
        fs::create_directories(out_dir);
    }

    write_float_tiff(out_dir / "x.tif", xImg);
    write_float_tiff(out_dir / "y.tif", yImg);
    write_float_tiff(out_dir / "z.tif", zImg);

    // --- 4. meta.json -----------------------------------------------------
    // Matches the fields QuadSurface::save() actually writes: format, type,
    // uuid, bbox (3D, of valid points), scale (grid pixels per (ux,uy) unit,
    // i.e. 1/step).
    const std::string uuid = opts.uuid.empty() ? make_uuid_v4() : opts.uuid;
    const double scale_x = 1.0 / opts.step_x;
    const double scale_y = 1.0 / opts.step_y;

    std::ostringstream json;
    json << "{\n";
    json << "    \"format\": \"tifxyz\",\n";
    json << "    \"type\": \"seg\",\n";
    json << "    \"uuid\": \"" << uuid << "\",\n";
    json << "    \"bbox\": [\n";
    json << "        [" << bboxLo[0] << ", " << bboxLo[1] << ", " << bboxLo[2] << "],\n";
    json << "        [" << bboxHi[0] << ", " << bboxHi[1] << ", " << bboxHi[2] << "]\n";
    json << "    ],\n";
    json << "    \"scale\": [" << scale_x << ", " << scale_y << "]\n";
    json << "}\n";

    const fs::path metaTmp = out_dir / "meta.json.tmp";
    {
        std::ofstream o(metaTmp);
        if (!o) throw std::runtime_error("failed to open " + metaTmp.string());
        o << json.str();
    }
    fs::rename(metaTmp, out_dir / "meta.json"); // atomic-ish, mirrors VC3D's own save()

    std::cerr << "wrote " << width << "x" << height << " tifxyz patch to "
              << out_dir << " (uuid " << uuid << ")\n";

    return uuid;
}

// ---------------------------------------------------------------------
// CLI: read points from a CSV file and convert
// ---------------------------------------------------------------------

std::vector<Point> read_points_bin(const fs::path &path)
{
	FILE *f = fopen(path.string().c_str(),"r");
	gridPointStruct p;

    std::vector<Point> points;

	if (f)
	{
		fseek(f,0,SEEK_END);
		long fsize = ftell(f);
		fseek(f,0,SEEK_SET);
  
		// input in x,y,z order 
		while(ftell(f)<fsize)
		{
			fread(&p,sizeof(p),1,f);

			Point pp;
			pp.ux = p.x; pp.uy = p.y; pp.vx = p.px; pp.vy = p.py; pp.vz = p.pz;
            points.push_back(pp);
		}
		
		fclose(f);
	}

    return points;
}

void print_usage(const char *argv0)
{
    std::cerr << "usage: " << argv0
              << " <points.csv> <out_dir> [--uuid STRING] [--step SX SY] [--no-overwrite]\n";
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const fs::path pointsPath = argv[1];
    const fs::path outDir = argv[2];

    TifxyzOptions opts;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--uuid" && i + 1 < argc) {
            opts.uuid = argv[++i];
        } else if (arg == "--step" && i + 2 < argc) {
            opts.step_x = std::stod(argv[++i]);
            opts.step_y = std::stod(argv[++i]);
        } else if (arg == "--no-overwrite") {
            opts.force_overwrite = false;
        } else {
            std::cerr << "unrecognized argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    try {
        const std::vector<Point> points = read_points_bin(pointsPath);
        if (points.empty()) {
            std::cerr << "no valid points parsed from " << pointsPath << "\n";
            return 1;
        }
        write_tifxyz(points, outDir, opts);
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}