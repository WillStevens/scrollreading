#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// PatchSpringSimulation
// ----------------------
// A direct C++ port of patchsprings.py, encapsulated in a single class.
//
// Differences from the original Python script:
//  - The Tkinter-based interactive window has been replaced with a
//    `show()` virtual method that (by default) rasterizes each frame to a
//    PPM image on disk. Override `show()` in a subclass if you want to
//    plug in a real windowing/graphics library (SFML, Qt, etc).
//  - `parameters.py` is replaced by the `Parameters` namespace
//    (see Parameters.hpp) - fill in your real values there.
//  - Python dict insertion order (relied on by SavePatches' iteration
//    order) is reproduced explicitly via an insertion-order vector,
//    since std::unordered_map does not guarantee ordering.
class PatchSpringSimulation {
public:
    // A 2D affine transform stored as (a,b,c,d,e,f), matching the
    // 6-tuple convention used throughout the original Python script:
    //   x' = a*x + b*y + c
    //   y' = d*x + e*y + f
    using Transform = std::array<double, 6>;

    explicit PatchSpringSimulation(double quadmeshSize,
                                    std::string outputDir);
    virtual ~PatchSpringSimulation() = default;

    // ---- Setup -------------------------------------------------------

    // alignmentOrder: each inner vector is one whitespace-split line from
    // the alignment order file (already tokenized, matching
    // `l.split(' ')[:-1]` from the Python version - i.e. the trailing
    // empty token from a trailing space has already been dropped).
    // patchLimit: stop loading once this many distinct patches have been
    // encountered (mirrors the global `patchLimit` used in Python).
    void loadPatches(const std::vector<std::vector<std::string>>& alignmentOrder,
                      int patchLimit);

    // Loads a CSV file of "patchNum,x,y,z" lines into patchVolCoords.
    void loadPatchVolCoords(const std::string& filename);

    // Writes <outputDir>/patchPositions.txt, one "patchNum x y angle" row
    // per loaded patch, in insertion order.
    void savePatches() const;

    // ---- Simulation ----------------------------------------------------

    // Runs one simulation step: connection forces, integrate motion,
    // bookkeeping (matches the body of Python's RunIteration, minus the
    // Tkinter `window.after` rescheduling).
    void runIteration();

    // Convenience driver: repeatedly calls runIteration() (and show())
    // until `maxIterations` steps have been taken.
    void run(int maxIterations);

    // Renders the current state. Default implementation writes a PPM
    // image to "<outputDir>/frame_<iteration>.ppm". Override to hook up
    // a real graphics/windowing toolkit.
    virtual void show(bool links = true) const;

    int iterationCount() const { return iterationCount_; }
    std::size_t patchCount() const { return patches_.size(); }

private:
    struct Patch {
        double x, y, a;      // current position/orientation
        double radius;
        double ga;            // "global angle" offset used when composing a+ga
        double origX, origY, origA; // original position, for measuring drift
    };

    struct Velocity {
        double vx, vy, va;
    };

    struct Connection {
        int p1, p2;
        double dist;
        double angle1, angle2;
    };

    // ---- Math helpers (free functions in the Python version) ---------
    static Transform composeTransforms(const Transform& t1, const Transform& t2);
    static Transform invertTransform(const Transform& t);
    static void affineTxToXYA(const Transform& t, double& x, double& y, double& a);
    static double distance(double x1, double y1, double x2, double y2);
    static double distance3D(double x1, double y1, double z1,
                              double x2, double y2, double z2);
    static double addAngle(double a, double b);

    // ---- Force / integration steps ------------------------------------
    void connectionForces();

    // Naive O(n^2) exclusion force pass (ExclusionForcesV1 in Python).
    void exclusionForcesNaive();

    // Grid-accelerated exclusion force pass (ExclusionForces in Python).
    // Not called by default from runIteration(), matching the original
    // script where the call is commented out.
    void exclusionForcesGrid();

    void move();

    // ---- State ----------------------------------------------------------
    std::vector<Patch> patches_;
    std::vector<Velocity> patchVel_;
    std::vector<Velocity> patchAcc_;
    std::vector<Connection> connections_;

    std::unordered_map<int, Transform> transformLookup_;
    std::unordered_map<int, int> patchIndexLookup_;
    std::vector<int> patchInsertionOrder_; // preserves Python dict insertion order
    std::unordered_map<int, std::array<double, 3>> patchVolCoords_;

    int iterationCount_ = 0;
    double quadmeshSize_;
    double radiusFactor_; // QUADMESH_SIZE/2.0 - kept for parity; unused elsewhere
    std::string outputDir_;

    static constexpr double CONNECT_FORCE_CONSTANT = 0.01;
    static constexpr double ANGLE_FORCE_CONSTANT = 0.01;
    static constexpr double PS_FRICTION_CONSTANT = 0.60;
    static constexpr double ANGLE_FRICTION_CONSTANT = 0.60;
    static constexpr double MAX_CORRECTABLE_DISTANCE = 3500.0;
};
