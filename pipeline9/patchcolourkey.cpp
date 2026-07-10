#include <tiffio.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>

// ── Minimal 5×7 bitmap font for digits 0–9 and space ──────────────────────
// Each digit is 5 pixels wide, 7 pixels tall, stored as 7 bytes (one per row,
// bit 4 = leftmost pixel).
static const uint8_t FONT[11][7] = {
    { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E }, // 0
    { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E }, // 1
    { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F }, // 2
    { 0x1F,0x02,0x04,0x02,0x01,0x11,0x0E }, // 3
    { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 }, // 4
    { 0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E }, // 5
    { 0x06,0x08,0x10,0x1E,0x11,0x11,0x0E }, // 6
    { 0x1F,0x01,0x02,0x04,0x08,0x08,0x08 }, // 7
    { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E }, // 8
    { 0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C }, // 9
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, // space (index 10)
};

static int digitIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return 10; // space
}

// Font cell size and scale factor
static const int FONT_W    = 5;
static const int FONT_H    = 7;
static const int FONT_SCALE = 2;          // renders at 10×14 per character
static const int CHAR_W    = FONT_W  * FONT_SCALE;
static const int CHAR_H    = FONT_H  * FONT_SCALE;

// ── Layout constants ───────────────────────────────────────────────────────
static const int BLOCK_SIZE  = 32;        // colour swatch
static const int GAP         = 4;         // gap between swatch and text
static const int MAX_DIGITS  = 8;         // enough for up to 9 999 999 + sign
static const int TEXT_W      = MAX_DIGITS * CHAR_W;
static const int CELL_W      = BLOCK_SIZE + GAP + TEXT_W;
static const int CELL_H      = BLOCK_SIZE;
static const int COLS        = 8;         // patches per row (adjust as needed)
static const int PADDING     = 4;         // image border

// ── Colour formula ─────────────────────────────────────────────────────────
struct RGB { uint8_t r, g, b; };

static RGB patchColour(int ci) {
    uint8_t r = 255 - 80*(ci%3)         - (ci/105)%79;
    uint8_t g = 255 - 40*((3*ci)%5)     - (ci/105)%37;
    uint8_t b = 255 - 26*((5*ci)%7)     - (ci/105)%23;
    return {r, g, b};
}

// ── Draw helpers ───────────────────────────────────────────────────────────
static void fillRect(std::vector<uint8_t>& img, int imgW,
                     int x, int y, int w, int h, RGB col)
{
    for (int row = y; row < y + h; ++row)
        for (int colm = x; colm < x + w; ++colm) {
            int idx = (row * imgW + colm) * 3;
            img[idx]   = col.r;
            img[idx+1] = col.g;
            img[idx+2] = col.b;
        }
}

static void drawChar(std::vector<uint8_t>& img, int imgW,
                     int x, int y, char c, RGB fg)
{
    const uint8_t* bitmap = FONT[digitIndex(c)];
    for (int row = 0; row < FONT_H; ++row) {
        for (int col = 0; col < FONT_W; ++col) {
            bool on = (bitmap[row] >> (FONT_W - 1 - col)) & 1;
            if (!on) continue;
            // Scale up
            for (int sy = 0; sy < FONT_SCALE; ++sy)
                for (int sx = 0; sx < FONT_SCALE; ++sx) {
                    int px = x + col*FONT_SCALE + sx;
                    int py = y + row*FONT_SCALE + sy;
                    int idx = (py * imgW + px) * 3;
                    img[idx]   = fg.r;
                    img[idx+1] = fg.g;
                    img[idx+2] = fg.b;
                }
        }
    }
}

static void drawString(std::vector<uint8_t>& img, int imgW,
                       int x, int y, const std::string& s, RGB fg)
{
    for (size_t i = 0; i < s.size(); ++i)
        drawChar(img, imgW, x + (int)i * CHAR_W, y, s[i], fg);
}

// ── Main function ──────────────────────────────────────────────────────────
void writePatchColourKey(const std::vector<int>& patches,
                         const std::string&      filename)
{
    int n    = (int)patches.size();
    int rows = (n + COLS - 1) / COLS;

    int imgW = PADDING + COLS * CELL_W  + (COLS  - 1) * GAP + PADDING;
    int imgH = PADDING + rows * CELL_H  + (rows  - 1) * GAP + PADDING;

    // Black background
    std::vector<uint8_t> img(imgW * imgH * 3, 0);

    for (int i = 0; i < n; ++i) {
        int ci   = patches[i];
        int col  = i % COLS;
        int row  = i / COLS;

        int cellX = PADDING + col * (CELL_W + GAP);
        int cellY = PADDING + row * (CELL_H + GAP);

        // Colour swatch
        RGB colour = patchColour(ci);
        fillRect(img, imgW, cellX, cellY, BLOCK_SIZE, BLOCK_SIZE, colour);

        // Patch number, vertically centred in the cell
        std::string label = std::to_string(ci);
        int textX = cellX + BLOCK_SIZE + GAP;
        int textY = cellY + (CELL_H - CHAR_H) / 2;
        drawString(img, imgW, textX, textY, label, {colour.r, colour.g, colour.b});
    }

    // ── Write TIFF ─────────────────────────────────────────────────────────
    TIFF* tif = TIFFOpen(filename.c_str(), "w");
    if (!tif) return;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,      imgW);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH,     imgH);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,   8);
    TIFFSetField(tif, TIFFTAG_ORIENTATION,     ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,     PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,    TIFFDefaultStripSize(tif, imgW * 3));

    for (int row = 0; row < imgH; ++row)
        TIFFWriteScanline(tif, img.data() + row * imgW * 3, row, 0);

    TIFFClose(tif);
}