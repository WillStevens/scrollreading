#include "PatchSpringSimulation.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

namespace {
constexpr double PI = 3.14159265358979323846;
}

// ------------------------------------------------------------------------
// Construction
// ------------------------------------------------------------------------

PatchSpringSimulation::PatchSpringSimulation(double quadmeshSize, std::string outputDir)
    : quadmeshSize_(quadmeshSize),
      radiusFactor_(quadmeshSize / 2.0),
      outputDir_(std::move(outputDir)) {}

// ------------------------------------------------------------------------
// Math helpers
// ------------------------------------------------------------------------

PatchSpringSimulation::Transform PatchSpringSimulation::composeTransforms(
    const Transform& t1, const Transform& t2) {
    double a = t1[0] * t2[0] + t1[1] * t2[3];
    double b = t1[0] * t2[1] + t1[1] * t2[4];
    double c = t1[0] * t2[2] + t1[1] * t2[5] + t1[2];
    double d = t1[3] * t2[0] + t1[4] * t2[3];
    double e = t1[3] * t2[1] + t1[4] * t2[4];
    double f = t1[3] * t2[2] + t1[4] * t2[5] + t1[5];
    return {a, b, c, d, e, f};
}

// For this nice inversion formula see
// https://negativeprobability.blogspot.com/2011/11/affine-transformations-and-their.html
PatchSpringSimulation::Transform PatchSpringSimulation::invertTransform(const Transform& t) {
    double a = t[0];
    double b = -t[1];
    double d = -t[3];
    double e = t[4];

    double c = -t[2] * t[0] + t[5] * t[1];
    double f = -t[5] * t[0] - t[2] * t[1];
    return {a, b, c, d, e, f};
}

void PatchSpringSimulation::affineTxToXYA(const Transform& t, double& x, double& y, double& a) {
    a = std::atan2(t[3], t[0]);
    x = t[2];
    y = t[5];
}

double PatchSpringSimulation::distance(double x1, double y1, double x2, double y2) {
    return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

double PatchSpringSimulation::distance3D(double x1, double y1, double z1,
                                          double x2, double y2, double z2) {
    return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1));
}

double PatchSpringSimulation::addAngle(double a, double b) {
    double r = a + b;
    if (r > PI) return r - 2.0 * PI;
    if (r <= -PI) return r + 2.0 * PI;
    return r;
}

// ------------------------------------------------------------------------
// Loading
// ------------------------------------------------------------------------

void PatchSpringSimulation::loadPatches(
    const std::vector<std::vector<std::string>>& alignmentOrder, int patchLimit) {
    std::unordered_set<int> patchNums;
    patches_.clear();
    patchVel_.clear();
    patchAcc_.clear();
    connections_.clear();
    transformLookup_.clear();
    patchIndexLookup_.clear();
    patchInsertionOrder_.clear();

    bool badPatch = false;
    [[maybe_unused]] bool newPatch = true;
    int patchNum = -1; // set by the first (other == -1) line, as in the original script

    for (const auto& l : alignmentOrder) {
		if (l.size()<=0)
			continue;
	    printf("size %d\n",(int)l.size());
        double radius = std::stod(l[1]);
        int other = std::stoi(l[2]);
        std::array<double, 6> tx{};
        for (int i = 0; i < 6; ++i) tx[i] = std::stod(l[3 + i]);
        double ta = tx[0], tb = tx[1], tc = tx[2], td = tx[3], te = tx[4], tf = tx[5];

        if (other == -1) {
            patchNum = std::stoi(l[0]);
            double x, y, a;
            affineTxToXYA({ta, tb, tc, td, te, tf}, x, y, a);
            patches_.push_back({x, y, a, radius, 0.0, x, y, a});
            patchVel_.push_back({0, 0, 0});
            patchAcc_.push_back({0, 0, 0});
            transformLookup_[patchNum] = {1, 0, 0, 0, 1, 0};
            patchIndexLookup_[patchNum] = static_cast<int>(patches_.size()) - 1;
            patchInsertionOrder_.push_back(patchNum);
            patchNums.insert(patchNum);
			printf("Added patch %d\n",patchNum);
        } else {
            int linePatchNum = std::stoi(l[0]);
            if (linePatchNum != patchNum) {
                badPatch = false;
                newPatch = true;
            } else {
                newPatch = false;
            }
            if (badPatch) continue;

            patchNum = linePatchNum;
            if (patchNums.find(patchNum) == patchNums.end() &&
                static_cast<int>(patchNums.size()) == patchLimit) {
                std::cout << "Stopped loading when " << patchNum << " encountered\n";
                int maxPatch = *std::max_element(patchNums.begin(), patchNums.end());
                std::cout << "Max patch number:" << maxPatch << "\n";
                return;
            }

            auto otherTransformIt = transformLookup_.find(other);
            if (otherTransformIt != transformLookup_.end()) {
                const Transform& otherTransform = otherTransformIt->second;
                Transform transform = composeTransforms(otherTransform, {ta, tb, tc, td, te, tf});

                double offsetX = transform[2] - otherTransform[2];
                double offsetY = transform[5] - otherTransform[5];

                double locationAngle = std::atan2(offsetY, offsetX);
                double angle = std::atan2(transform[3], transform[0]); // global orientation
                double dist = std::sqrt(offsetX * offsetX + offsetY * offsetY);

                int otherIndex = patchIndexLookup_[other];

                if (patchNums.find(patchNum) == patchNums.end()) {
                    transformLookup_[patchNum] = transform;
                    patchNums.insert(patchNum);
                    patchIndexLookup_[patchNum] = static_cast<int>(patches_.size());
                    patches_.push_back({transform[2], transform[5], 0.0, radius, angle,
                                         transform[2], transform[5], 0.0});
                    patchVel_.push_back({0, 0, 0});
                    patchAcc_.push_back({0, 0, 0});
                    patchInsertionOrder_.push_back(patchNum);
					printf("Added patch %d\n",patchNum);
                } else {
                    double newDistance = distance(patches_.back().x, patches_.back().y,
                                                   transform[2], transform[5]);
                    std::cout << "Patchnum " << patchNum << ", other " << other
                              << ", distance " << newDistance << "\n";
                    if (newDistance > MAX_CORRECTABLE_DISTANCE) {
                        std::cout << "Patch " << patchNum << " is a bad patch\n\n";
                        badPatch = true;
                        int lastPatchIndex = static_cast<int>(patches_.size()) - 1;
                        while (!connections_.empty() && connections_.back().p1 == lastPatchIndex) {
                            connections_.pop_back();
                        }
                        patches_.pop_back();
                        patchVel_.pop_back();
                        patchAcc_.pop_back();
                        patchIndexLookup_.erase(patchNum);
                        if (!patchInsertionOrder_.empty() &&
                            patchInsertionOrder_.back() == patchNum) {
                            patchInsertionOrder_.pop_back();
                        }
                    }
                }
                if (!badPatch) {
                    connections_.push_back({static_cast<int>(patches_.size()) - 1, otherIndex,
                                             dist, addAngle(PI, locationAngle), locationAngle});
                }
            } else {
                std::cout << "Patch " << patchNum << " can't find other patch " << other
                          << " (perhaps other was a bad patch)\n";
            }
        }
    }

    if (!patchNums.empty()) {
        int maxPatch = *std::max_element(patchNums.begin(), patchNums.end());
        std::cout << "Max patch number:" << maxPatch << "\n";
    }
}

void PatchSpringSimulation::loadPatchVolCoords(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) {
        std::cerr << "Could not open " << filename << "\n";
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, ',')) row.push_back(token);
        if (row.size() == 4) {
            int idx = std::stoi(row[0]);
            patchVolCoords_[idx] = {std::stod(row[1]), std::stod(row[2]), std::stod(row[3])};
        }
    }
}

void PatchSpringSimulation::savePatches() const {
    std::cout << "Saving positions...\n";
    std::ofstream f(outputDir_ + "/patchPositions.txt");
    if (f) {
        for (int patchNum : patchInsertionOrder_) {
            auto it = patchIndexLookup_.find(patchNum);
            if (it == patchIndexLookup_.end()) continue;
            const Patch& p = patches_[it->second];
            f << patchNum << " " << p.x << " " << p.y << " " << addAngle(p.a, p.ga) << "\n";
        }
    }
    std::cout << "Saved\n";
}

// ------------------------------------------------------------------------
// Forces / integration
// ------------------------------------------------------------------------

void PatchSpringSimulation::connectionForces() {
    for (const auto& conn : connections_) {
        int p1 = conn.p1, p2 = conn.p2;
        double dist = conn.dist, angle1 = conn.angle1, angle2 = conn.angle2;

        double x1 = patches_[p1].x, y1 = patches_[p1].y;
        double x2 = patches_[p2].x, y2 = patches_[p2].y;

        double currentAngle12 = addAngle(patches_[p1].a, angle1);
        double currentAngle21 = addAngle(patches_[p2].a, angle2);

        double reqx2 = x1 + std::cos(currentAngle12) * dist;
        double reqy2 = y1 + std::sin(currentAngle12) * dist;
        double reqx1 = x2 + std::cos(currentAngle21) * dist;
        double reqy1 = y2 + std::sin(currentAngle21) * dist;

        double actualAngle12 = std::atan2(y2 - y1, x2 - x1);
        double adiff1 = addAngle(actualAngle12, -currentAngle12);

        double actualAngle21 = std::atan2(y1 - y2, x1 - x2);
        double adiff2 = addAngle(actualAngle21, -currentAngle21);

        patchAcc_[p1].vx += (reqx1 - x1) * CONNECT_FORCE_CONSTANT - (reqx2 - x2) * CONNECT_FORCE_CONSTANT;
        patchAcc_[p1].vy += (reqy1 - y1) * CONNECT_FORCE_CONSTANT - (reqy2 - y2) * CONNECT_FORCE_CONSTANT;
        patchAcc_[p1].va += adiff1 * ANGLE_FORCE_CONSTANT - adiff2 * ANGLE_FORCE_CONSTANT;

        patchAcc_[p2].vx += (reqx2 - x2) * CONNECT_FORCE_CONSTANT - (reqx1 - x1) * CONNECT_FORCE_CONSTANT;
        patchAcc_[p2].vy += (reqy2 - y2) * CONNECT_FORCE_CONSTANT - (reqy1 - y1) * CONNECT_FORCE_CONSTANT;
        patchAcc_[p2].va += adiff2 * ANGLE_FORCE_CONSTANT - adiff1 * ANGLE_FORCE_CONSTANT;
    }
}

// Two patches should not be nearer in 2D space than they are in 3D space -
// apply a repulsive force if so. Naive O(n^2) version (ExclusionForcesV1).
void PatchSpringSimulation::exclusionForcesNaive() {
    int acounter = 0;
    std::vector<int> patchNums;
    patchNums.reserve(patchIndexLookup_.size());
    for (const auto& kv : patchIndexLookup_) patchNums.push_back(kv.first);

    for (std::size_t i = 0; i < patchNums.size(); ++i) {
        for (std::size_t j = i + 1; j < patchNums.size(); ++j) {
            int iIndex = patchIndexLookup_[patchNums[i]];
            int jIndex = patchIndexLookup_[patchNums[j]];
            double ix = patches_[iIndex].x, iy = patches_[iIndex].y;
            double jx = patches_[jIndex].x, jy = patches_[jIndex].y;
            const auto& ipos3D = patchVolCoords_.at(patchNums[i]);
            const auto& jpos3D = patchVolCoords_.at(patchNums[j]);

            double dist2D = distance(ix, iy, jx, jy) * quadmeshSize_;
            double dist3D = distance3D(ipos3D[0], ipos3D[1], ipos3D[2],
                                        jpos3D[0], jpos3D[1], jpos3D[2]);

            if (dist2D > 0 && dist2D < dist3D) {
                ++acounter;
                double force = (dist3D - dist2D) * 0.001;
                double fx = (ix - jx) * force / dist2D;
                double fy = (iy - jy) * force / dist2D;

                patchAcc_[iIndex].vx += fx;
                patchAcc_[iIndex].vy += fy;
                patchAcc_[jIndex].vx -= fx;
                patchAcc_[jIndex].vy -= fy;
            }
        }
    }
    std::cout << "Number found=" << acounter << "\n";
}

// Grid-accelerated exclusion force pass (ExclusionForces in Python). Not
// invoked by default from runIteration(), matching the original script
// (the call there is commented out).
void PatchSpringSimulation::exclusionForcesGrid() {
    int acounter = 0;
    const double uGridSizeX = 10.0;
    const double uGridSizeY = 10.0;

    struct Cell {
        double minX, minY, maxX, maxY;
        double minVX, minVY, minVZ, maxVX, maxVY, maxVZ;
        std::unordered_set<int> members;
    };

    // Grid key -> cell. Using a map keyed on (gx,gy) pair via long combination.
    std::map<std::pair<long long, long long>, Cell> gridLookup;

    std::vector<int> patchNums;
    patchNums.reserve(patchIndexLookup_.size());
    for (const auto& kv : patchIndexLookup_) patchNums.push_back(kv.first);

    auto floorDiv = [](double a, double b) {
        return static_cast<long long>(std::floor(a / b));
    };

    for (int pn : patchNums) {
        int idx = patchIndexLookup_[pn];
        double ix = patches_[idx].x, iy = patches_[idx].y;
        const auto& ipos3D = patchVolCoords_.at(pn);

        auto key = std::make_pair(floorDiv(ix, uGridSizeX), floorDiv(iy, uGridSizeY));
        auto it = gridLookup.find(key);
        if (it == gridLookup.end()) {
            Cell c;
            c.minX = c.maxX = ix;
            c.minY = c.maxY = iy;
            c.minVX = c.maxVX = ipos3D[0];
            c.minVY = c.maxVY = ipos3D[1];
            c.minVZ = c.maxVZ = ipos3D[2];
            c.members.insert(pn);
            gridLookup[key] = std::move(c);
        } else {
            Cell& c = it->second;
            c.minX = std::min(c.minX, ix);
            c.maxX = std::max(c.maxX, ix);
            c.minY = std::min(c.minY, iy);
            c.maxY = std::max(c.maxY, iy);
            c.minVX = std::min(c.minVX, ipos3D[0]);
            c.maxVX = std::max(c.maxVX, ipos3D[0]);
            c.minVY = std::min(c.minVY, ipos3D[1]);
            c.maxVY = std::max(c.maxVY, ipos3D[1]);
            c.minVZ = std::min(c.minVZ, ipos3D[2]);
            c.maxVZ = std::max(c.maxVZ, ipos3D[2]);
            c.members.insert(pn);
        }
    }

    std::vector<std::pair<long long, long long>> gridKeys;
    gridKeys.reserve(gridLookup.size());
    for (const auto& kv : gridLookup) gridKeys.push_back(kv.first);

    for (std::size_t i = 0; i < gridKeys.size(); ++i) {
        for (std::size_t j = i; j < gridKeys.size(); ++j) {
            const Cell& cell1 = gridLookup[gridKeys[i]];
            const Cell& cell2 = gridLookup[gridKeys[j]];

            double deltax2D = std::max({0.0, cell2.minX - cell1.maxX, cell1.minX - cell2.maxX});
            double deltay2D = std::max({0.0, cell2.minY - cell1.maxY, cell1.minY - cell2.maxY});
            double dmin2D = std::sqrt(deltax2D * deltax2D + deltay2D * deltay2D);

            double deltax3D = std::max(cell1.maxVX, cell2.maxVX) - std::min(cell1.minVX, cell2.minVX);
            double deltay3D = std::max(cell1.maxVY, cell2.maxVY) - std::min(cell1.minVY, cell2.minVY);
            double deltaz3D = std::max(cell1.maxVZ, cell2.maxVZ) - std::min(cell1.minVZ, cell2.minVZ);
            double dmax3D = std::sqrt(deltax3D * deltax3D + deltay3D * deltay3D + deltaz3D * deltaz3D);

            if (dmin2D * quadmeshSize_ < dmax3D) {
                for (int id : cell1.members) {
                    for (int jd : cell2.members) {
                        if (gridKeys[i] != gridKeys[j] || jd > id) {
                            int iIndex = patchIndexLookup_[id];
                            int jIndex = patchIndexLookup_[jd];
                            double ix = patches_[iIndex].x, iy = patches_[iIndex].y;
                            double jx = patches_[jIndex].x, jy = patches_[jIndex].y;
                            const auto& ipos3D = patchVolCoords_.at(id);
                            const auto& jpos3D = patchVolCoords_.at(jd);

                            double dist2D = distance(ix, iy, jx, jy) * quadmeshSize_;
                            double dist3D = distance3D(ipos3D[0], ipos3D[1], ipos3D[2],
                                                        jpos3D[0], jpos3D[1], jpos3D[2]);

                            if (dist2D > 0 && dist2D < dist3D) {
                                ++acounter;
                                double force = (dist3D - dist2D) * 0.001;
                                double fx = (ix - jx) * force / dist2D;
                                double fy = (iy - jy) * force / dist2D;

                                patchAcc_[iIndex].vx += fx;
                                patchAcc_[iIndex].vy += fy;
                                patchAcc_[jIndex].vx -= fx;
                                patchAcc_[jIndex].vy -= fy;
                            }
                        }
                    }
                }
            }
        }
    }
    std::cout << "Number found=" << acounter << "\n";
}

void PatchSpringSimulation::move() {
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        patches_[i].x += patchVel_[i].vx;
        patches_[i].y += patchVel_[i].vy;
        patches_[i].a += patchVel_[i].va;

        patchVel_[i].vx = patchAcc_[i].vx + patchVel_[i].vx;
        patchVel_[i].vy = patchAcc_[i].vy + patchVel_[i].vy;
        patchVel_[i].va = patchAcc_[i].va + patchVel_[i].va;

        patchVel_[i].vx *= PS_FRICTION_CONSTANT;
        patchVel_[i].vy *= PS_FRICTION_CONSTANT;
        patchVel_[i].va *= ANGLE_FRICTION_CONSTANT;

        patchAcc_[i] = {0.0, 0.0, 0.0};
    }
}

// ------------------------------------------------------------------------
// Rendering (PPM stand-in for the Tkinter canvas)
// ------------------------------------------------------------------------

namespace {
double truncate01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}
}

void PatchSpringSimulation::show(bool links) const {
    const int width = 1350, height = 700;
    std::vector<unsigned char> img(static_cast<std::size_t>(width) * height * 3, 255);

    auto setPixel = [&](int px, int py, unsigned char r, unsigned char g, unsigned char b) {
        if (px < 0 || px >= width || py < 0 || py >= height) return;
        std::size_t o = (static_cast<std::size_t>(py) * width + px) * 3;
        img[o] = r;
        img[o + 1] = g;
        img[o + 2] = b;
    };

    auto drawLine = [&](double x0, double y0, double x1, double y1) {
        int steps = static_cast<int>(std::max(std::abs(x1 - x0), std::abs(y1 - y0))) + 1;
        for (int s = 0; s <= steps; ++s) {
            double t = steps == 0 ? 0.0 : static_cast<double>(s) / steps;
            setPixel(static_cast<int>(x0 + (x1 - x0) * t), static_cast<int>(y0 + (y1 - y0) * t),
                     0, 0, 0);
        }
    };

    auto drawFilledCircle = [&](double cx, double cy, double r, unsigned char cr,
                                 unsigned char cg, unsigned char cb) {
        int rr = static_cast<int>(std::ceil(std::abs(r)));
        for (int dy = -rr; dy <= rr; ++dy) {
            for (int dx = -rr; dx <= rr; ++dx) {
                if (dx * dx + dy * dy <= rr * rr) {
                    setPixel(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, cr, cg, cb);
                }
            }
        }
    };

    const double offsetX = 600, offsetY = 300;
    const double scale = 0.2;
    const double rotate = -1.2;
    std::size_t patchesLen = patches_.size();

    std::vector<std::pair<double, double>> screenPos(patchesLen);

    for (std::size_t i = 0; i < patchesLen; ++i) {
        const Patch& p = patches_[i];
        double x = p.x * std::cos(rotate) - p.y * std::sin(rotate);
        double y = p.y * std::cos(rotate) + p.x * std::sin(rotate);

        double patchif = PI * static_cast<double>(i) / static_cast<double>(patchesLen);
        double red = 1.0 + std::cos(patchif);
        double green = 1.0 - std::cos(patchif);
        double blue = 1.0 * std::sin(patchif);
        red = truncate01(red / 2.0);
        green = truncate01(green / 2.0);
        blue = truncate01(blue);

        double sx = offsetX + x * scale;
        double sy = offsetY + y * scale;
        screenPos[i] = {sx, sy};

        drawFilledCircle(sx, sy, p.radius * scale, static_cast<unsigned char>(red * 255),
                          static_cast<unsigned char>(green * 255),
                          static_cast<unsigned char>(blue * 255));
    }

    if (links) {
        for (const auto& conn : connections_) {
            const Patch& p0 = patches_[conn.p1];
            double x0 = p0.x * std::cos(rotate) - p0.y * std::sin(rotate);
            double y0 = p0.y * std::cos(rotate) + p0.x * std::sin(rotate);
            double a0 = p0.a + rotate;
            double sx0 = offsetX + x0 * scale, sy0 = offsetY + y0 * scale;
            drawLine(sx0, sy0, sx0 + p0.radius * 0.8 * std::cos(a0 + conn.angle1) * scale,
                     sy0 + p0.radius * 0.8 * std::sin(a0 + conn.angle1) * scale);

            const Patch& p1 = patches_[conn.p2];
            double x1 = p1.x * std::cos(rotate) - p1.y * std::sin(rotate);
            double y1 = p1.y * std::cos(rotate) + p1.x * std::sin(rotate);
            double a1 = p1.a + rotate;
            double sx1 = offsetX + x1 * scale, sy1 = offsetY + y1 * scale;
            drawLine(sx1, sy1, sx1 + p1.radius * 0.8 * std::cos(a1 + conn.angle2) * scale,
                     sy1 + p1.radius * 0.8 * std::sin(a1 + conn.angle2) * scale);
        }
    }

    std::string filename = outputDir_ + "/frame_" + std::to_string(iterationCount_) + ".ppm";
    std::ofstream f(filename, std::ios::binary);
    if (f) {
        f << "P6\n" << width << " " << height << "\n255\n";
        f.write(reinterpret_cast<const char*>(img.data()), img.size());
    }
}

// ------------------------------------------------------------------------
// Main loop
// ------------------------------------------------------------------------

void PatchSpringSimulation::runIteration() {
    connectionForces();
    // exclusionForcesGrid(); // left disabled, matching the original script
    move();

    ++iterationCount_;

    if (iterationCount_ == 50) {
        savePatches();
        std::vector<std::pair<int, double>> movements;
        for (int patchNum : patchInsertionOrder_) {
            auto it = patchIndexLookup_.find(patchNum);
            if (it == patchIndexLookup_.end()) continue;
            const Patch& p = patches_[it->second];
            movements.emplace_back(patchNum, distance(p.origX, p.origY, p.x, p.y));
        }
        std::sort(movements.begin(), movements.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        std::size_t start = movements.size() > 100 ? movements.size() - 100 : 0;
        for (std::size_t i = start; i < movements.size(); ++i) {
            std::cout << "Patch " << movements[i].first << " moved " << movements[i].second
                      << "\n";
        }
    }

    show();
}

void PatchSpringSimulation::run(int maxIterations) {
    for (int i = 0; i < maxIterations; ++i) {
        runIteration();
    }
}
