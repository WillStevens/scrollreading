/*
 * patchsprings.cpp
 *
 * C++ conversion of patchsprings.py
 * Ball-and-spring simulation for refining patch positions.
 * ASCII graphics replace the tkinter/PIL display.
 *
 * Build:   g++ -O2 -std=c++17 -o patchsprings patchsprings.cpp -lm
 * Usage:   patchsprings <alignmentOrderFile> <N>
 *
 *   alignmentOrderFile  – text file describing patch relationships
 *                         (same format as the Python version)
 *   N                   – maximum number of patches to load
 */

/* Converted from python version (patchsprings.py) using Claude Sonnet 4.6 */

/* Prompt used prior to pasting code:

I have a python program that I want to convert to C++,  but not the graphical display. The graphical display can be replaced with some kind of ASCII graphics output - just to help verify it is working. The program simulates a collection of objects (called patches) connected together using a ball and spring model. The purpose of the simulation is to refine the position of the patches, based on the positions of other patches.
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

#include "parameters.h"

// ---------------------------------------------------------------------------
// Physics constants
// ---------------------------------------------------------------------------
static const double CONNECT_FORCE_CONSTANT  = 0.01;
static const double ANGLE_FORCE_CONSTANT    = 0.01;
static const double FRICTION_CONSTANT       = 0.60;
static const double ANGLE_FRICTION_CONSTANT = 0.60;
static const double RADIUS_FACTOR           = QUADMESH_SIZE / 2.0;
static const double MAX_CORRECTABLE_DISTANCE = 3500.0;

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

// Patch state: (x, y, angle, radius, globalAngle)
struct Patch {
    double x, y, a, radius, ga;
};

// Velocity / acceleration share the same 3-tuple layout (x, y, angle)
struct Vec3 {
    double x, y, a;
};

// A spring connection between two patch indices
struct Connection {
    int    p1, p2;
    double dist;
    double angle1;   // angle relative to p1's orientation
    double angle2;   // angle relative to p2's orientation
};

// Affine transform stored as 6 coefficients (row-major 2×3)
// | t[0]  t[1]  t[2] |
// | t[3]  t[4]  t[5] |
struct Transform {
    double v[6];
};

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static int iterationCount = 0;
static int patchLimit     = 0;

static std::vector<Patch>      patches;
static std::vector<Vec3>       patchVel;
static std::vector<Vec3>       patchAcc;
static std::vector<Connection> connections;

static std::map<int, Transform> transformLookup;
static std::map<int, int>       patchIndexLookup;

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------

static double addAngle(double a, double b)
{
    double r = a + b;
    if (r >  M_PI) return r - 2.0 * M_PI;
    if (r <= -M_PI) return r + 2.0 * M_PI;
    return r;
}

static double distance2D(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1, dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

static Transform composeTransforms(const Transform& t1, const Transform& t2)
{
    Transform r;
    r.v[0] = t1.v[0]*t2.v[0] + t1.v[1]*t2.v[3];
    r.v[1] = t1.v[0]*t2.v[1] + t1.v[1]*t2.v[4];
    r.v[2] = t1.v[0]*t2.v[2] + t1.v[1]*t2.v[5] + t1.v[2];
    r.v[3] = t1.v[3]*t2.v[0] + t1.v[4]*t2.v[3];
    r.v[4] = t1.v[3]*t2.v[1] + t1.v[4]*t2.v[4];
    r.v[5] = t1.v[3]*t2.v[2] + t1.v[4]*t2.v[5] + t1.v[5];
    return r;
}

// Extract (angle, tx, ty) from an affine transform
static void affineTxToXYA(const Transform& t, double& angle, double& tx, double& ty)
{
    angle = std::atan2(t.v[3], t.v[0]);
    tx    = t.v[2];
    ty    = t.v[5];
}

// ---------------------------------------------------------------------------
// LoadPatches
// ---------------------------------------------------------------------------
// Each line of alignmentOrder has the format (space-separated):
//   patchNum  radius  other  ta  tb  tc  td  te  tf
// When other == -1 the patch is placed absolutely; otherwise it is placed
// relative to patch <other>.

static void loadPatches(const std::vector<std::vector<std::string>>& alignmentOrder)
{
    std::set<int> patchNums;
    int  currentPatchNum = -1;
    bool badPatch        = false;
    bool newPatch        = true;

    for (const auto& l : alignmentOrder) {
        if (l.size() < 9) continue;   // skip malformed lines

        int    patchNum = std::stoi(l[0]);
        double radius   = std::stod(l[1]);
        int    other    = std::stoi(l[2]);
        Transform localT;
        for (int i = 0; i < 6; ++i) localT.v[i] = std::stod(l[3 + i]);

        // ----- Root patch (other == -1) -----
        if (other == -1) {
            double angle, tx, ty;
            affineTxToXYA(localT, angle, tx, ty);

            patches  .push_back({tx, ty, angle, radius, 0.0});
            patchVel .push_back({0, 0, 0});
            patchAcc .push_back({0, 0, 0});

            Transform identity = {1,0,0, 0,1,0};
            transformLookup[patchNum]  = identity;
            patchIndexLookup[patchNum] = (int)patches.size() - 1;
            patchNums.insert(patchNum);

            currentPatchNum = patchNum;
            badPatch = false;
            newPatch = true;
            continue;
        }

        // ----- Relative patch -----
        if (patchNum != currentPatchNum) {
            badPatch        = false;
            newPatch        = true;
            currentPatchNum = patchNum;
        } else {
            newPatch = false;
        }

        if (badPatch) continue;

        // Stop if we have already loaded patchLimit patches
        if (patchNums.find(patchNum) == patchNums.end() &&
            (int)patchNums.size() == patchLimit) {
            std::printf("Stopped loading when %d encountered\n", patchNum);
            return;
        }

        // Check that the reference patch is known
        if (transformLookup.find(other) == transformLookup.end()) {
            std::printf("Patch %d can't find other patch %d "
                        "(perhaps other was a bad patch)\n", patchNum, other);
            continue;
        }

        const Transform& otherTransform = transformLookup[other];
        Transform transform = composeTransforms(otherTransform, localT);

        double offsetX = transform.v[2] - otherTransform.v[2];
        double offsetY = transform.v[5] - otherTransform.v[5];

        double locationAngle = std::atan2(offsetY, offsetX);
        double globalAngle   = std::atan2(transform.v[3], transform.v[0]);
        double dist          = std::sqrt(offsetX*offsetX + offsetY*offsetY);

        int otherIndex = patchIndexLookup[other];

        if (patchNums.find(patchNum) == patchNums.end()) {
            // New patch – add it
            transformLookup[patchNum]  = transform;
            patchNums.insert(patchNum);
            patchIndexLookup[patchNum] = (int)patches.size();

            patches  .push_back({transform.v[2], transform.v[5], 0.0, radius, globalAngle});
            patchVel .push_back({0, 0, 0});
            patchAcc .push_back({0, 0, 0});
        } else {
            // Already seen – check it hasn't diverged
            int idx = patchIndexLookup[patchNum];
            if (distance2D(patches[idx].x, patches[idx].y,
                           transform.v[2], transform.v[5]) > MAX_CORRECTABLE_DISTANCE) {
                std::printf("Patch %d is a bad patch\n", patchNum);
                badPatch = true;

                // Roll back connections that reference this patch
                while (!connections.empty() && connections.back().p1 == idx) {
                    connections.pop_back();
                }
                patches.erase(patches.begin() + idx);
                patchVel.erase(patchVel.begin() + idx);
                patchAcc.erase(patchAcc.begin() + idx);
                patchIndexLookup.erase(patchNum);
                continue;
            }
        }

        if (!badPatch) {
            int p1idx = patchIndexLookup[patchNum];
            connections.push_back({
                p1idx,
                otherIndex,
                dist,
                addAngle(M_PI, locationAngle),
                locationAngle
            });
        }
    }
}

// ---------------------------------------------------------------------------
// SavePatches
// ---------------------------------------------------------------------------
static void savePatches()
{
    std::printf("Saving positions...\n");
    std::string path = OUTPUT_DIR + "/patchPositions.txt";
    std::ofstream f(path);
    if (!f) {
        std::fprintf(stderr, "Could not open %s for writing\n", path.c_str());
        return;
    }
    for (const auto& [patchNum, idx] : patchIndexLookup) {
        const Patch& p = patches[idx];
        f << patchNum << " "
          << p.x << " "
          << p.y << " "
          << addAngle(p.a, p.ga) << "\n";
    }
    f.close();
    std::printf("Saved to %s\n", path.c_str());
}

// ---------------------------------------------------------------------------
// ConnectionForces  –  accumulates accelerations from spring constraints
// ---------------------------------------------------------------------------
static void connectionForces()
{
    for (const auto& c : connections) {
        const Patch& P1 = patches[c.p1];
        const Patch& P2 = patches[c.p2];

        double currentAngle12 = addAngle(P1.a, c.angle1);
        double currentAngle21 = addAngle(P2.a, c.angle2);

        double reqx2 = P1.x + std::cos(currentAngle12) * c.dist;
        double reqy2 = P1.y + std::sin(currentAngle12) * c.dist;
        double reqx1 = P2.x + std::cos(currentAngle21) * c.dist;
        double reqy1 = P2.y + std::sin(currentAngle21) * c.dist;

        double actualAngle12 = std::atan2(P2.y - P1.y, P2.x - P1.x);
        double adiff1 = addAngle(actualAngle12, -currentAngle12);

        double actualAngle21 = std::atan2(P1.y - P2.y, P1.x - P2.x);
        double adiff2 = addAngle(actualAngle21, -currentAngle21);

        patchAcc[c.p1].x += (reqx1 - P1.x) * CONNECT_FORCE_CONSTANT
                           - (reqx2 - P2.x) * CONNECT_FORCE_CONSTANT;
        patchAcc[c.p1].y += (reqy1 - P1.y) * CONNECT_FORCE_CONSTANT
                           - (reqy2 - P2.y) * CONNECT_FORCE_CONSTANT;
        patchAcc[c.p1].a += adiff1 * ANGLE_FORCE_CONSTANT
                           - adiff2 * ANGLE_FORCE_CONSTANT;

        patchAcc[c.p2].x += (reqx2 - P2.x) * CONNECT_FORCE_CONSTANT
                           - (reqx1 - P1.x) * CONNECT_FORCE_CONSTANT;
        patchAcc[c.p2].y += (reqy2 - P2.y) * CONNECT_FORCE_CONSTANT
                           - (reqy1 - P1.y) * CONNECT_FORCE_CONSTANT;
        patchAcc[c.p2].a += adiff2 * ANGLE_FORCE_CONSTANT
                           - adiff1 * ANGLE_FORCE_CONSTANT;
    }
}

// ---------------------------------------------------------------------------
// Move  –  integrate positions and apply friction
// ---------------------------------------------------------------------------
static void move()
{
    for (int i = 0; i < (int)patches.size(); ++i) {
        patches[i].x += patchVel[i].x;
        patches[i].y += patchVel[i].y;
        patches[i].a += patchVel[i].a;

        patchVel[i].x = (patchAcc[i].x + patchVel[i].x) * FRICTION_CONSTANT;
        patchVel[i].y = (patchAcc[i].y + patchVel[i].y) * FRICTION_CONSTANT;
        patchVel[i].a = (patchAcc[i].a + patchVel[i].a) * ANGLE_FRICTION_CONSTANT;

        patchAcc[i] = {0.0, 0.0, 0.0};
    }
}

// ---------------------------------------------------------------------------
// ASCII display
// ---------------------------------------------------------------------------
// Renders patches as dots/letters on a character grid.
// A simple rotation is applied to match the Python Show() function
// (rotate = -1.2 radians).

static void showASCII()
{
    static const int   W      = 120;
    static const int   H      = 40;
    static const double ROTATE = -1.2;
    static const double SCALE  =  0.1;   // tweak to suit your data scale

    // Centre the display
    static const double CX = W / 2.0;
    static const double CY = H / 2.0;

    char grid[H][W + 1];
    for (int r = 0; r < H; ++r) {
        std::memset(grid[r], ' ', W);
        grid[r][W] = '\0';
    }

    // Draw connections first (underscores / dashes)
    for (const auto& c : connections) {
        const Patch& pa = patches[c.p1];
        const Patch& pb = patches[c.p2];

        double ax = pa.x * std::cos(ROTATE) - pa.y * std::sin(ROTATE);
        double ay = pa.y * std::cos(ROTATE) + pa.x * std::sin(ROTATE);
        double bx = pb.x * std::cos(ROTATE) - pb.y * std::sin(ROTATE);
        double by = pb.y * std::cos(ROTATE) + pb.x * std::sin(ROTATE);

        int col1 = (int)(CX + ax * SCALE);
        int row1 = (int)(CY + ay * SCALE);
        int col2 = (int)(CX + bx * SCALE);
        int row2 = (int)(CY + by * SCALE);

        // Simple Bresenham line
        int dc = col2 - col1, dr = row2 - row1;
        int steps = std::max(std::abs(dc), std::abs(dr));
        if (steps == 0) continue;
        for (int s = 0; s <= steps; ++s) {
            int cc = col1 + dc * s / steps;
            int rr = row1 + dr * s / steps;
            if (cc >= 0 && cc < W && rr >= 0 && rr < H) {
                if (grid[rr][cc] == ' ') grid[rr][cc] = '.';
            }
        }
    }

    // Draw patches (coloured by index using ASCII shading chars)
    static const char symbols[] = "oO0@#*+x";
    int n = (int)patches.size();
    int pi = 0;
    for (const auto& p : patches) {
        double rx = p.x * std::cos(ROTATE) - p.y * std::sin(ROTATE);
        double ry = p.y * std::cos(ROTATE) + p.x * std::sin(ROTATE);

        int col = (int)(CX + rx * SCALE);
        int row = (int)(CY + ry * SCALE);

        if (col >= 0 && col < W && row >= 0 && row < H) {
            grid[row][col] = symbols[(pi * 8 / n) % 8];
        }
        ++pi;
    }

    // Print frame
    std::printf("\033[2J\033[H");   // ANSI: clear screen, move cursor home
    std::printf("+");
    for (int c = 0; c < W; ++c) std::putchar('-');
    std::printf("+\n");
    for (int r = 0; r < H; ++r) {
        std::printf("|%s|\n", grid[r]);
    }
    std::printf("+");
    for (int c = 0; c < W; ++c) std::putchar('-');
    std::printf("+\n");
    std::printf("Iteration: %d   Patches: %zu   Connections: %zu\n",
                iterationCount, patches.size(), connections.size());
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// Run one simulation step
// ---------------------------------------------------------------------------
static void runIteration()
{
    connectionForces();
    move();
    ++iterationCount;

    if (iterationCount == 50) {
        savePatches();
    }
}

// ---------------------------------------------------------------------------
// Input file parsing
// ---------------------------------------------------------------------------
static std::vector<std::vector<std::string>> readAlignmentOrder(const std::string& filename)
{
    std::vector<std::vector<std::string>> result;
    std::ifstream f(filename);
    if (!f) {
        std::fprintf(stderr, "Cannot open file: %s\n", filename.c_str());
        std::exit(1);
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (ss >> tok) tokens.push_back(tok);
        if (!tokens.empty()) result.push_back(tokens);
    }
    return result;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::fprintf(stderr,
            "Usage: patchsprings <alignmentOrderFile> <N>\n"
            "  alignmentOrderFile  file describing patch relationships\n"
            "  N                   maximum number of patches to load\n");
        return 1;
    }

    std::string filename = argv[1];
    patchLimit           = std::atoi(argv[2]);

    auto alignmentOrder = readAlignmentOrder(filename);

    loadPatches(alignmentOrder);

    std::printf("Loaded %zu patches, %zu connections.\n",
                patches.size(), connections.size());

    // Run simulation loop (press Ctrl-C to stop)
    // Display is refreshed every 10 ms, same cadence as the Python version.
    while (true) {
        runIteration();
        showASCII();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}