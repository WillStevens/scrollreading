// kernel_test.cpp: the two AVX-512 kernels of the growth against the scalar functions they stand for, both taken from
// the object files of the same build (patch_generator.o, calcvectorfield.o), so the scalar side is the production
// scalar code compiled with the production flags. Build with `make kernel_test`; it needs a CPU with AVX-512F.
//
//   forces: PatchGenerator::ForcesOnPoint over activeList[ai0 .. ai0+n) against ForcesOnPoint16(ai0, n), n 1..16,
//           on random sheets; compared: the WHOLE velSheet array after each batch (every float, bit pattern) and
//           maxForce (bit pattern).
//   field:  VectorFieldCalculator::ComputeFieldDense over n taps of a 5x5x5 window against ComputeFieldDense16 on the
//           same taps, on random 9x9x9 blocks; compared: the three floats of all 125 entries of value[] (bit pattern).
// Usage: kernel_test <points per kernel> <seed> <out.csv>     for example: ./kernel_test 1000000 20260926 bits.csv
// Writes one row per kernel: tool,kernel,points,batches,tail_sizes_seen,compared_floats,differing_floats,
// nan_results,first_difference. Exit status 1 when any float differs.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <ostream>
#include <iostream>
#define private public
#include "patch_generator.h"
#undef private

int patches_in_flight = 1;   // defined in simpaper10.cpp, read by the pool sizing

static std::mt19937_64 rng;
static double U(double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); }
static int I(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

struct Row { std::string kernel; long points = 0, batches = 0, compared = 0, differing = 0, nans = 0; std::set<int> tails; std::string first; };

// ---- forces ----------------------------------------------------------------------------------
static void RandomSheet(PatchGenerator *g)
{
	const int S = SHEET_SIZE;
	double density = U(0.3, 1.0);
	double noise = std::vector<double>{0.001, 0.05, 0.5, 2.0, 4.0}[I(0, 4)];
	double ox = U(-300, 300), oy = U(-300, 300), oz = U(-300, 300);
	double vfmag = std::vector<double>{0.0, 0.05, 0.41, 1.0}[I(0, 3)];
	double velmag = std::vector<double>{0.0, 0.01, 0.3, 2.0}[I(0, 3)];
	for(int x = 0; x<S; x++) for(int y = 0; y<S; y++)
	{
		g->active[x][y] = (x > 0 && y > 0 && x < S-1 && y < S-1) && U(0, 1) < density;
		g->paperSheet[x][y].pos = Vec3((float)(ox + (x - S/2)*QUADMESH_SIZE + U(-noise, noise)),
		                               (float)(oy + (y - S/2)*QUADMESH_SIZE*0.3 + U(-noise, noise)),
		                               (float)(oz + (y - S/2)*QUADMESH_SIZE*0.95 + U(-noise, noise)));
		g->vfSheet[x][y] = Vec3((float)U(-vfmag, vfmag), (float)U(-vfmag, vfmag), (float)U(-vfmag, vfmag));
		g->velSheet[x][y] = Vec3((float)U(-velmag, velmag), (float)U(-velmag, velmag), (float)U(-velmag, velmag));
	}
	// degenerate cases: a neighbour on top of the point (distance 0, 0/0), and rare NaN / inf inputs
	int ncoinc = I(0, 40);
	for(int k = 0; k<ncoinc; k++)
	{
		int x = I(2, S-3), y = I(2, S-3);
		g->paperSheet[x+I(-1, 1)][y+I(-1, 1)].pos = g->paperSheet[x][y].pos;
	}
	if (U(0, 1) < 0.2)
	{
		int nbad = I(1, 5);
		for(int k = 0; k<nbad; k++)
		{
			int x = I(1, S-2), y = I(1, S-2);
			float bad = std::vector<float>{NAN, -NAN, INFINITY, -INFINITY, 1e30f}[I(0, 4)];
			switch(I(0, 2))
			{
				case 0: g->paperSheet[x][y].pos.x = bad; break;
				case 1: g->vfSheet[x][y].y = bad; break;
				default: g->velSheet[x][y].z = bad; break;
			}
		}
	}
	// the active list: the active cells, in random order
	g->activeListSize = 0;
	std::vector<std::pair<int,int>> cells;
	for(int x = 0; x<S; x++) for(int y = 0; y<S; y++) if (g->active[x][y]) cells.push_back({x, y});
	std::shuffle(cells.begin(), cells.end(), rng);
	for(auto &c : cells) { g->activeList[g->activeListSize][0] = c.first; g->activeList[g->activeListSize++][1] = c.second; }
}

static void TestForces(long points, Row &r)
{
	r.kernel = "ForcesOnPoint16";
	PatchGenerator *g = new PatchGenerator("unused");
	g->silent = true;
	const size_t VN = sizeof(g->velSheet) / sizeof(float);
	std::vector<float> vel0(VN), velS(VN);
	int sinceSheet = 1 << 30;
	while (r.points < points)
	{
		if (sinceSheet > 400) { RandomSheet(g); sinceSheet = 0; }
		sinceSheet++;
		if (g->activeListSize < 1) continue;
		int n = I(1, 16);
		if (n > g->activeListSize) n = g->activeListSize;
		int ai0 = I(0, g->activeListSize - n);
		float m0 = std::vector<float>{0.0f, 0.0f, 0.0f, (float)U(0, 1), (float)U(0, 1e-4), NAN}[I(0, 5)];
		memcpy(vel0.data(), &g->velSheet[0][0], VN*4);
		float mS = m0;
		for(int ai = ai0; ai < ai0+n; ai++) g->ForcesOnPoint(ai, mS);
		memcpy(velS.data(), &g->velSheet[0][0], VN*4);
		memcpy(&g->velSheet[0][0], vel0.data(), VN*4);
		float mV = m0;
		g->ForcesOnPoint16(ai0, n, mV);
		const float *velV = &g->velSheet[0][0].x;
		for(size_t k = 0; k<VN; k++)
			if (bits(velS[k]) != bits(velV[k]))
			{
				if (r.differing == 0) { char b[200]; snprintf(b, sizeof b, "velSheet float %zu: scalar %08x vector %08x (batch %ld n %d)", k, bits(velS[k]), bits(velV[k]), r.batches, n); r.first = b; }
				r.differing++;
			}
		if (bits(mS) != bits(mV))
		{
			if (r.differing == 0) { char b[200]; snprintf(b, sizeof b, "maxForce: scalar %08x vector %08x (batch %ld n %d)", bits(mS), bits(mV), r.batches, n); r.first = b; }
			r.differing++;
		}
		r.compared += VN + 1;
		for(int ai = ai0; ai < ai0+n; ai++)
		{
			const Vec3 &v = g->velSheet[g->activeList[ai][0]][g->activeList[ai][1]];
			r.nans += std::isnan(v.x) + std::isnan(v.y) + std::isnan(v.z);
		}
		r.nans += std::isnan(mV);
		// keep the scalar result as the next state, so velocities evolve like a relaxation
		memcpy(&g->velSheet[0][0], velS.data(), VN*4);
		r.points += n; r.batches++; if (n < 16) r.tails.insert(n);
	}
	delete g;
}

// ---- field -----------------------------------------------------------------------------------
static void TestField(long points, Row &r)
{
	r.kernel = "ComputeFieldDense16";
	VectorFieldCalculator vfc(NULL);
	const int RADIUS = SMOOTH_WINDOW + VF_RAD, SIDE = 2*RADIUS + 1, L = 2*SMOOTH_WINDOW + 1, N = L*L*L;
	unsigned char dense[SIDE*SIDE*SIDE + 4];
	const int VS[3] = {VOL_SIZE_X, VOL_SIZE_Y, VOL_SIZE_Z};
	while (r.points < points)
	{
		// the centre: anywhere, near the volume's low or high edge (where the dense path still applies), or next to
		// a chunk border (multiples of 128 and of 192)
		int c[3];
		for(int a = 0; a<3; a++)
		{
			int lo = RADIUS - (a==0 ? VOL_OFFSET_X : a==1 ? VOL_OFFSET_Y : VOL_OFFSET_Z);
			int hi = VS[a] - 1 - RADIUS - (a==0 ? VOL_OFFSET_X : a==1 ? VOL_OFFSET_Y : VOL_OFFSET_Z);
			switch(I(0, 3))
			{
				case 0: c[a] = I(lo, hi); break;
				case 1: c[a] = lo + I(0, 3); break;
				case 2: c[a] = hi - I(0, 3); break;
				default: { int step = I(0, 1) ? 128 : 192; int k = I(1, (hi - lo) / step - 1); c[a] = lo + k*step + I(-5, 5); }
			}
			c[a] = std::max(lo, std::min(hi, c[a]));
		}
		int x = c[0], y = c[1], z = c[2];
		int oz = z+VOL_OFFSET_Z-RADIUS, oy = y+VOL_OFFSET_Y-RADIUS, ox = x+VOL_OFFSET_X-RADIUS;
		// the block: empty, full, noise of a density, a plane through it, or arbitrary bytes
		int kind = I(0, 4);
		double p = U(0, 1);
		double nx = U(-1, 1), ny = U(-1, 1), nz = U(-1, 1), off = U(-3, 3);
		for(int k = 0; k<SIDE*SIDE*SIDE; k++)
		{
			int zz = k / (SIDE*SIDE) - RADIUS, yy = (k / SIDE) % SIDE - RADIUS, xx = k % SIDE - RADIUS;
			unsigned char b;
			switch(kind)
			{
				case 0: b = 0; break;
				case 1: b = 255; break;
				case 2: b = U(0, 1) < p ? 255 : 0; break;
				case 3: b = (nx*xx + ny*yy + nz*zz > off) ? 255 : 0; break;
				default: b = (unsigned char)I(0, 255) < 128 ? 0 : (unsigned char)I(0, 3); break;
			}
			dense[k] = b;
		}
		for(int k = 0; k<4; k++) dense[SIDE*SIDE*SIDE + k] = (unsigned char)I(0, 255);   // padding: must not matter
		int cx[N], cy[N], cz[N];
		int k = 0;
		for(int zo = z-SMOOTH_WINDOW; zo<=z+SMOOTH_WINDOW; zo++)
		for(int yo = y-SMOOTH_WINDOW; yo<=y+SMOOTH_WINDOW; yo++)
		for(int xo = x-SMOOTH_WINDOW; xo<=x+SMOOTH_WINDOW; xo++) { cx[k] = xo; cy[k] = yo; cz[k] = zo; k++; }
		// the taps to compute: a random subset of the window in window order (as toDo is), cut into batches of 16
		std::vector<int> todo;
		double take = U(0.05, 1.0);
		for(int i = 0; i<N; i++) if (U(0, 1) < take) todo.push_back(i);
		if (todo.empty()) continue;
		Vec3 vS[N], vV[N];
		for(int i = 0; i<N; i++) { Vec3 init = U(0, 1) < 0.1 ? Vec3((float)U(-1, 1), NAN, 0.0f) : Vec3(); vS[i] = init; vV[i] = init; }
		for(size_t j = 0; j<todo.size(); j++) { int i = todo[j]; vfc.ComputeFieldDense(cx[i], cy[i], cz[i], vS[i], dense, oz, oy, ox, SIDE, SIDE, SIDE); }
		for(size_t j = 0; j<todo.size(); j += 16)
		{
			int n = (int)std::min<size_t>(16, todo.size() - j);
			vfc.ComputeFieldDense16(cx, cy, cz, vV, todo.data() + j, n, dense, oz, oy, ox, SIDE, SIDE, SIDE);
			r.batches++; if (n < 16) r.tails.insert(n);
		}
		for(int i = 0; i<N; i++)
		{
			const float s[3] = {vS[i].x, vS[i].y, vS[i].z}, v[3] = {vV[i].x, vV[i].y, vV[i].z};
			for(int q = 0; q<3; q++)
			{
				if (bits(s[q]) != bits(v[q]))
				{
					if (r.differing == 0) { char b[240]; snprintf(b, sizeof b, "value[%d].%c at (%d,%d,%d): scalar %08x vector %08x (block kind %d)", i, "xyz"[q], cx[i], cy[i], cz[i], bits(s[q]), bits(v[q]), kind); r.first = b; }
					r.differing++;
				}
				r.nans += std::isnan(v[q]);
			}
		}
		r.compared += 3*N;
		r.points += todo.size();
	}
}

int main(int argc, char **argv)
{
	if (argc != 4) { fprintf(stderr, "usage: kernel_test <points per kernel> <seed> <out.csv>\n"); return 2; }
	if (!__builtin_cpu_supports("avx512f")) { fprintf(stderr, "no avx512f: refused\n"); return 3; }
	setenv("SIMPAPER_FORCE_THREADS", "1", 1);
	long points = atol(argv[1]);
	rng.seed(strtoull(argv[2], NULL, 10));
	Row a, b;
	TestForces(points, a);
	TestField(points, b);
	FILE *f = fopen(argv[3], "w");
	if (!f) return 4;
	fprintf(f, "# written by kernel_test (seed %s, %ld points per kernel)\n", argv[2], points);
	fprintf(f, "tool,kernel,points,batches,tail_sizes_seen,compared_floats,differing_floats,nan_results,first_difference\n");
	for(Row *r : {&a, &b})
	{
		std::string t; for(int s : r->tails) { if (!t.empty()) t += " "; t += std::to_string(s); }
		fprintf(f, "kernel_test,%s,%ld,%ld,%s,%ld,%ld,%ld,\"%s\"\n", r->kernel.c_str(), r->points,
		        r->batches, t.c_str(), r->compared, r->differing, r->nans, r->first.c_str());
		printf("%s: %ld points, %ld batches, tails [%s], %ld floats compared, %ld differing, %ld NaN results%s%s\n",
		       r->kernel.c_str(), r->points, r->batches, t.c_str(), r->compared, r->differing, r->nans,
		       r->differing ? "; first: " : "", r->first.c_str());
	}
	fclose(f);
	return (a.differing || b.differing) ? 1 : 0;
}
