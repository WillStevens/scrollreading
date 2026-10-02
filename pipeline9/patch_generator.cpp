#include <iostream>
#include <cmath>

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <array>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>

// Timers, to see where the time inside growth goes. They are instrumentation: they write to
// stdout and touch no computation. perf is not usable here because perf_event_paranoid is 4 and
// we are not root.
#include <omp.h>
// A7.9: these counters used to be plain globals, incremented inside parallel regions over
// patches. They affect no stored value, but they make the PRINTOUT unreliable as soon as more
// than one patch is in flight, and a diagnostic that lies is worse than one that is missing:
// we rely on these numbers to decide where to optimise.
// Here there is one per thread, on separate cache lines, summed only when printing.
#define COUNTERS_MAX 64
struct PatchCounters
{
	double t_forces, t_setvf, t_stress, t_newpts, t_total;
	double t_neighbours, t_makepts, t_addpts;
	long n_precomputed, n_taps, n_failed, n_warmed;
	long n_neighbour_calls, n_comparisons;
	long n_relax_par, n_relax_ser, n_relax_points;
	char pad[64];
};
static PatchCounters counters[COUNTERS_MAX] = {};
static inline PatchCounters &mine() { return counters[omp_get_thread_num() & (COUNTERS_MAX-1)]; }
long n_hit = 0, n_miss = 0;   // these two are used by calcvectorfield.cpp as well

#ifndef GROWTH_TIMERS
#define GROWTH_TIMERS 0
#endif
static inline double now(void)
{
#if GROWTH_TIMERS
	struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
	return t.tv_sec + t.tv_nsec*1e-9;
#else
	return 0.0;
#endif
}
extern "C" long zarrStoreHits_1, zarrStoreInPlace_1, zarrStoreMisses_1, zarrStoreHeld_1, zarrStoreInPlace_1;

void PrintTimers(void)
{
	// sum of the per-thread counters
	double s_t_forces = 0, s_t_setvf = 0, s_t_stress = 0, s_t_newpts = 0, s_t_total = 0;
	double s_t_neighbours = 0, s_t_makepts = 0, s_t_addpts = 0;
	long s_n_precomputed = 0, s_n_taps = 0, s_n_failed = 0, s_n_warmed = 0;
	long s_n_neighbour_calls = 0, s_n_comparisons = 0;
	long s_par = 0, s_ser = 0, s_pts = 0;
	for(int q = 0; q < COUNTERS_MAX; q++)
	{
		const PatchCounters &c = counters[q];
		s_t_forces += c.t_forces; s_t_setvf += c.t_setvf; s_t_stress += c.t_stress;
		s_t_newpts += c.t_newpts; s_t_total += c.t_total; s_t_neighbours += c.t_neighbours;
		s_t_makepts += c.t_makepts; s_t_addpts += c.t_addpts;
		s_n_precomputed += c.n_precomputed; s_n_taps += c.n_taps; s_n_failed += c.n_failed;
		s_n_warmed += c.n_warmed; s_n_neighbour_calls += c.n_neighbour_calls;
		s_n_comparisons += c.n_comparisons;
		s_par += c.n_relax_par; s_ser += c.n_relax_ser; s_pts += c.n_relax_points;
	}
	if (s_t_total <= 0) return;
	printf("\nGROWTH TIMERS: total %.1f s\n", s_t_total);
	printf("  forces (the two per-point loops) %7.1f s  %5.1f %%\n", s_t_forces, 100*s_t_forces/s_t_total);
	printf("  SetVectorField (zarr reads)      %7.1f s  %5.1f %%\n", s_t_setvf, 100*s_t_setvf/s_t_total);
	printf("  MarkHighStress                %7.1f s  %5.1f %%\n", s_t_stress, 100*s_t_stress/s_t_total);
	printf("  new points                       %7.1f s  %5.1f %%\n", s_t_newpts, 100*s_t_newpts/s_t_total);
	printf("    MakeNewPoints              %7.1f s  %5.1f %%\n", s_t_makepts, 100*s_t_makepts/s_t_total);
	printf("    AddNewPoints               %7.1f s  %5.1f %%\n", s_t_addpts, 100*s_t_addpts/s_t_total);
	printf("    of which HasCloseNeighbour %7.1f s  %5.1f %%   (%ld calls, %.1f comparisons each)\n",
	       s_t_neighbours, 100*s_t_neighbours/s_t_total, s_n_neighbour_calls,
	       s_n_neighbour_calls ? (double)s_n_comparisons/s_n_neighbour_calls : 0.0);
	if (s_n_precomputed) printf("  shared precompute: %ld calls, %ld taps, %ld warmed serially, %ld failed (%.1f %%)\n",
	       s_n_precomputed, s_n_taps, s_n_warmed, s_n_failed, 100.0*s_n_failed/(s_n_taps-s_n_warmed+1));
	printf("  vector field cache: %ld hits, %ld computations, %.1f %% hit rate\n",
	       n_hit, n_miss, 100.0*n_hit/(n_hit+n_miss+1));
	{
		if (zarrStoreHits_1 + zarrStoreMisses_1)
			printf("  shared chunk store: %ld served (%ld read in place), %ld decompressed, %ld held (%.1f GB)\n",
			       zarrStoreHits_1, zarrStoreInPlace_1, zarrStoreMisses_1, zarrStoreHeld_1,
			       zarrStoreHeld_1*7.077888/1024.0);
	}
	if (s_par + s_ser)
		printf("  relaxation steps: %ld in parallel, %ld serial, %.0f active points on average, %.2f us of forces per step\n",
		       s_par, s_ser, (double)s_pts/(s_par+s_ser), 1e6*s_t_forces/(s_par+s_ser));
	printf("  rest                             %7.1f s  %5.1f %%\n",
	       s_t_total-s_t_forces-s_t_setvf-s_t_stress-s_t_newpts,
	       100*(s_t_total-s_t_forces-s_t_setvf-s_t_stress-s_t_newpts)/s_t_total);
}
#include <utility>

#include <smmintrin.h>

using namespace std;

#include "parameters.h"

#include "patch_generator.h"

// ---------------------------------------------------------------------------------------------
// The persistent pool behind SIMPAPER_FORCE_THREADS.
//
// One pool per PatchGenerator, that is per concurrent patch, created when the generator is and
// destroyed with it. The workers spin on a generation counter, fall back to yielding and then to
// sleeping on a condition variable when nothing has been asked of them for a while, so that a
// slot that is aligning rather than relaxing does not hold cores it is not using.
//
// Bit identity. Pass 0 runs ForcesOnPoint over a contiguous block of activeList and keeps its own
// maximum; pass 1 runs MovePoint over the same block. The per-point code is the code that was
// there before, called with the same arguments. The maxima are combined afterwards over workers
// 0, 1, ... in that order, and std::max on floats is exact, so the result does not depend on how
// the points were divided.
// ---------------------------------------------------------------------------------------------
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <immintrin.h>
#include <unistd.h>

#define FORCE_POOL_MAX 16

class PatchGenerator;

class ForcePool
{
public:
	PatchGenerator *owner = nullptr;
	int nw = 1;                       // workers including the caller
	int n = 0;                        // active points this step
	float partMax[FORCE_POOL_MAX];

	std::atomic<unsigned> generation{0};
	std::atomic<int> finished{0};
	std::atomic<int> sleepers{0};
	std::atomic<bool> stopping{false};
	std::atomic<int> atCount{0};
	std::atomic<unsigned char> sense{0};
	// One sense byte per participant, in the pool rather than in thread-local storage: the caller
	// of a pool is whichever OpenMP thread happens to be running that slot this batch, and it is
	// not the same one from batch to batch, so a sense kept per thread goes out of step with the
	// pool and the barrier hangs. Measured the hard way on 2026-09-14.
	unsigned char wSense[FORCE_POOL_MAX] = {0};
	std::vector<std::thread> threads;
	std::mutex m;
	std::condition_variable cv;

	void start(PatchGenerator *o, int workers);
	void stop();
	void run(int n_);                 // called by the owner thread
	void body(int w, int ph);         // one worker's share of one pass
	void barrier(int w);
	void loop(int w);
	~ForcePool() { stop(); }
};

extern int patches_in_flight;

// How many threads share one relaxation step. Measured on 2026-09-14 over blocks of three runs of
// 300 patches, 4 slots: serial 28.676 s, two threads 28.037 s, three 27.872 s, four 27.929 s, so
// three is where it stops paying, and the whole effect is 2.8 %. It costs 2.5 times the CPU time,
// because the threads spin while they wait, so the default is taken only when the machine has
// cores to spare: the chain is also run several processes at a time, and there the right answer is
// one thread per slot and no pool at all.
// SIMPAPER_FORCE_THREADS overrides, and 1 turns the pool off entirely.
static int forcePoolThreads(void)
{
	static int decided = -1;
	if (decided < 0)
	{
		const char *e = getenv("SIMPAPER_FORCE_THREADS");
		if (e) decided = atoi(e);
		else
		{
			long cores = sysconf(_SC_NPROCESSORS_ONLN);
			if (cores < 1) cores = 1;
			int slots = patches_in_flight > 0 ? patches_in_flight : 1;
			decided = (int)(cores / (2L * slots));   // half the machine left for everything else
			if (decided > 3) decided = 3;            // measured: four is not better than three
		}
		if (decided < 1) decided = 1;
		if (decided > FORCE_POOL_MAX) decided = FORCE_POOL_MAX;
	}
	return decided;
}

static int forcePoolThreshold(void)
{
	static int decided = -1;
	if (decided < 0)
	{
		const char *e = getenv("SIMPAPER_FORCE_MIN_POINTS");
		decided = e ? atoi(e) : 2048;
	}
	return decided;
}


#define MARGIN 8 // Don't try to fill near the edges of the volume

#define EXPECTED_DISTANCE(xd,yd) (QUADMESH_SIZE*sqrt(xd*xd+yd*yd))

#define NEIGHBOUR_RADIUS 2
#define NEIGHBOUR_RADIUS_FLOAT 2.0


// --- the pool, out of line because it calls back into PatchGenerator ---------------------------

void ForcePool::body(int w, int ph)
{
	// a contiguous block, decided by n and w alone
	int lo = (int)((long)n * w / nw), hi = (int)((long)n * (w+1) / nw);
	if (ph == 0)
	{
		float mx = 0.0f;
		owner->ForcesOnPoints(lo, hi, mx);
		partMax[w] = mx;
	}
	else
	{
		for(int ai = lo; ai < hi; ai++)
			owner->MovePoint(ai);
	}
}

void ForcePool::loop(int w)
{
	unsigned seen = 0;
	for(;;)
	{
		// Spin first, because a relaxation step is a few microseconds and a sleep costs more than
		// that. Then yield, then sleep: a slot that is aligning rather than relaxing must not hold
		// a core for the tens of milliseconds that takes.
		int spins = 0;
		while (generation.load(std::memory_order_acquire) == seen)
		{
			if (stopping.load(std::memory_order_acquire)) return;
			if (spins < 20000) { _mm_pause(); spins++; }
			else if (spins < 40000) { std::this_thread::yield(); spins++; }
			else
			{
				std::unique_lock<std::mutex> lk(m);
				sleepers.fetch_add(1, std::memory_order_release);
				cv.wait_for(lk, std::chrono::milliseconds(1));
				sleepers.fetch_sub(1, std::memory_order_release);
			}
		}
		seen = generation.load(std::memory_order_acquire);
		if (stopping.load(std::memory_order_acquire)) return;
		body(w,0);
		barrier(w);
		body(w,1);
		finished.fetch_add(1, std::memory_order_release);
	}
}

// A sense-reversing barrier over the nw participants, spun on. It sits between the two passes of
// one relaxation step, which is where the Jacobi update needs it: no point may move before every
// force has been read from the positions of the step it belongs to.
void ForcePool::barrier(int w)
{
	unsigned char mySense = (unsigned char)(wSense[w] ^ 1);
	wSense[w] = mySense;
	if (atCount.fetch_add(1, std::memory_order_acq_rel) == nw-1)
	{
		atCount.store(0, std::memory_order_relaxed);
		sense.store(mySense, std::memory_order_release);
	}
	else
	{
		int spins = 0;
		while (sense.load(std::memory_order_acquire) != mySense)
		{
			if (spins < 100000) { _mm_pause(); spins++; }
			else std::this_thread::yield();
		}
	}
}

void ForcePool::start(PatchGenerator *o, int workers)
{
	owner = o;
	nw = workers;
	if (nw <= 1) return;
	for(int w = 1; w < nw; w++)
		threads.emplace_back([this,w]{ this->loop(w); });
}

void ForcePool::stop()
{
	if (threads.empty()) return;
	stopping.store(true, std::memory_order_release);
	generation.fetch_add(1, std::memory_order_release);
	{ std::lock_guard<std::mutex> lk(m); }
	cv.notify_all();
	for(auto &t : threads) if (t.joinable()) t.join();
	threads.clear();
}

void ForcePool::run(int n_)
{
	n = n_;
	finished.store(0, std::memory_order_relaxed);
	generation.fetch_add(1, std::memory_order_release);
	if (sleepers.load(std::memory_order_acquire) > 0)
	{
		std::lock_guard<std::mutex> lk(m);
		cv.notify_all();
	}
	body(0,0);
	barrier(0);
	body(0,1);
	int spins = 0;
	while (finished.load(std::memory_order_acquire) < nw-1)
	{
		if (spins < 200000) { _mm_pause(); spins++; }
		else std::this_thread::yield();
	}
}

void PatchGenerator::MakeActive(int x, int y)
{
	active[x][y] = true;
	activeList[activeListSize][0] = x;
	activeList[activeListSize++][1] = y;
}


void PatchGenerator::ClearPointLookup(void)
{
	pointLookup.clear();
}

void PatchGenerator::AddPointToLookup(const point &p)
{
	pointInt q;
	q.x = ((int)p.x)/NEIGHBOUR_RADIUS;
	q.y = ((int)p.y)/NEIGHBOUR_RADIUS;
	q.z = ((int)p.z)/NEIGHBOUR_RADIUS;
	
	// This used to be three table lookups for the same bucket: count, then operator[] to create,
	// then operator[] to write. operator[] already creates the empty element when it is missing,
	// so one is enough.
	pointLookup[q].push_back(p);
}


float PatchGenerator::GetDistanceAtPoint(int xp, int yp, int zp)
{
	pointInt p;
	p.x = xp;
	p.y = yp;
	p.z = zp;
	
	if (distanceLookup.count(p) == 0)
	{
		distanceLookup[p] = 255-ZARRRead_1(surfaceZarr,zp,yp,xp);
	}
	return distanceLookup[p];
}

bool PatchGenerator::HasCloseNeighbour(const point &p)
{
	double tv0 = now(); mine().n_neighbour_calls++;
	struct Closer { double t0; Closer(double t):t0(t){} ~Closer(){ mine().t_neighbours += now()-t0; } } _c(tv0);
	int xp = ((int)p.x)/NEIGHBOUR_RADIUS;
	int yp = ((int)p.y)/NEIGHBOUR_RADIUS;
	int zp = ((int)p.z)/NEIGHBOUR_RADIUS;
	
	pointInt q;
	for(q.x = xp-1; q.x<=xp+1; q.x++)
	for(q.y = yp-1; q.y<=yp+1; q.y++)
	for(q.z = zp-1; q.z<=zp+2; q.z++)
	{
		// This used to be pointLookup[q] on a READ-ONLY path: operator[] inserts an empty vector
		// for every missing bucket, and this loop visits 36 of them per candidate point. The map
		// filled up with empty buckets, with rehashing and a dirty cache, and the function sits
		// inside MakeNewPoints, which the 2026-09-11 timers put at 51.3 % of growth time.
		// find() inserts nothing and the result is the same: iterating a freshly created empty
		// vector and skipping the bucket both mean "no neighbour". The map is never walked in
		// full, so the difference is not observable anywhere else.
		auto it = pointLookup.find(q);
		if (it == pointLookup.end()) continue;
		for(const point &r : it->second)
		{
			mine().n_comparisons++;
			if ((p-r).length()<=NEIGHBOUR_RADIUS_FLOAT)
			{
				return true;
			}
		}
	}

	return false;
}

void PatchGenerator::SetVectorField(int x, int y)
{
  uint64_t px = (uint64_t)paperSheet[x][y].pos.x;
  uint64_t py = (uint64_t)paperSheet[x][y].pos.y;
  uint64_t pz = (uint64_t)paperSheet[x][y].pos.z;
  uint64_t p = (px<<32)+(py<<16)+pz;
  
  const Vec3 *found = vectorFieldLookup.find(p);
  if (!found)
  {
	Vec3 v; // defaults to 0,0,0

    if (px-VOL_OFFSET_X>=0 && px-VOL_OFFSET_X<VOL_SIZE_X && py-VOL_OFFSET_Y>=0 && py-VOL_OFFSET_Y<VOL_SIZE_Y && pz-VOL_OFFSET_Z>=0 && pz-VOL_OFFSET_Z<VOL_SIZE_Z)
    {	
		
		/*v.x = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,0)*VECTORFIELD_CONSTANT;
		v.y = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,1)*VECTORFIELD_CONSTANT;
		v.z = ZARRRead_c64i1b256(vectorFieldZarr,pz,py,px,2)*VECTORFIELD_CONSTANT;*/
		vfc->GetSmoothedVectorFieldInt8(px,py,pz,v);
		v = v*VECTORFIELD_CONSTANT;
	}

	vectorFieldLookup.insert(p,v); vfSheet[x][y] = v;
  }
  else
  {
	vfSheet[x][y] = *found;
  }
  
  //printf("Vector field for %d,%d was %f,%f,%f,%f\n",x,y,paperVectorField[x][y][0],paperVectorField[x][y][1],paperVectorField[x][y][2],paperVectorField[x][y][3]);
}


void PatchGenerator::InitExpectedDistanceLookup(void)
{
  for(int x = 0; x<3; x++)
  for(int y = 0; y<3; y++)
  {
    expectedDistanceLookup[x][y] = QUADMESH_SIZE*sqrt((x-1)*(x-1)+(y-1)*(y-1));
  }
}

#define FORCES_INNERLOOP(xd,yd) \
      if (active[x+xd][y+yd]) \
	  { \
		Vec3 direction = posxy-paperSheet[x+xd][y+yd].pos; \
        float actualDistance = direction.length(); \
        direction /= actualDistance; \
        \
        float force = expectedDistanceLookup[xd+1][yd+1]-actualDistance; \
        \
        acc += direction*force; \
	  }

// Forces on one point: reads .pos of the neighbours, writes .vel of its own point, updates the
// running maximum. It touches nothing another point is writing in the same pass, which is a
// Jacobi update, so it can run in parallel without changing the result.
void PatchGenerator::ForcesOnPoint(int ai, float &maxForce)
{
	int x = activeList[ai][0], y = activeList[ai][1];

	Vec3 acc = vfSheet[x][y];
	Vec3 posxy = paperSheet[x][y].pos;

	// The order of the eight neighbours is left untouched: floating-point addition is not
	// associative.
	FORCES_INNERLOOP(-1,-1)
	FORCES_INNERLOOP(-1,0)
	FORCES_INNERLOOP(-1,1)
	FORCES_INNERLOOP(0,-1)
	FORCES_INNERLOOP(0,1)
	FORCES_INNERLOOP(1,-1)
	FORCES_INNERLOOP(1,0)
	FORCES_INNERLOOP(1,1)

	acc *= SPRING_FORCE_CONSTANT;

	float forceMag = acc.lengthSquared();
	maxForce = std::max(forceMag,maxForce);

	velSheet[x][y] *= FRICTION_CONSTANT;
	velSheet[x][y] += acc;
}

// ForcesOnPoint for the entries ai0 .. ai0+n-1 of activeList (1 <= n <= 16), one AVX-512 lane per
// point, structure of arrays built by gathers from the sheets, the tail by the mask 'all'.
// Each lane does ForcesOnPoint's operations in its order:
//   acc = vfSheet, posxy = pos; for the eight neighbours in FORCES_INNERLOOP's order, where the
//   neighbour is active: direction = posxy - pos(n); d = sqrt((dx*dx + dy*dy) + dz*dz);
//   direction /= d (three divisions); force = expected - d; acc += direction*force;
//   then acc *= (float)SPRING_FORCE_CONSTANT; forceMag = (ax*ax + ay*ay) + az*az;
//   vel = vel*(float)FRICTION_CONSTANT, then vel += acc.
// The maximum is folded lane by lane, in ai order, with the same std::max as the scalar loop, so
// even a NaN goes the way it goes there. Points are distinct cells, so the scatters never collide,
// and like the scalar body this reads only positions and writes only its own velocities.
// The file is compiled with -ffp-contract=off: no multiply and add here is fused.
static_assert(sizeof(paperPoint) == 3*sizeof(float), "paperPoint is gathered as three floats");
static_assert(sizeof(Vec3) == 3*sizeof(float), "Vec3 is gathered as three floats");

__attribute__((target("avx512f")))
void PatchGenerator::ForcesOnPoint16(int ai0, int n, float &maxForce)
{
	const __mmask16 all = (__mmask16)((1u << n) - 1u);
	const __m512i zi = _mm512_setzero_si512();
	const __m512 zf = _mm512_setzero_ps();
	const __m512i one = _mm512_set1_epi32(1), two = _mm512_set1_epi32(2), three = _mm512_set1_epi32(3);
	const __m512i lane2 = _mm512_setr_epi32(0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30);

	const int *al = &activeList[ai0][0];
	__m512i X = _mm512_mask_i32gather_epi32(zi, all, lane2, al, 4);
	__m512i Y = _mm512_mask_i32gather_epi32(zi, all, _mm512_add_epi32(lane2, one), al, 4);
	__m512i C = _mm512_add_epi32(_mm512_mullo_epi32(X, _mm512_set1_epi32(SHEET_SIZE)), Y);   // cell x*S+y
	__m512i C3 = _mm512_mullo_epi32(C, three);

	const float *P = &paperSheet[0][0].pos.x;
	const float *VF = &vfSheet[0][0].x;
	float *V = &velSheet[0][0].x;
	const int *ACT = (const int *)&active[0][0];

	__m512 ax = _mm512_mask_i32gather_ps(zf, all, C3, VF, 4);
	__m512 ay = _mm512_mask_i32gather_ps(zf, all, _mm512_add_epi32(C3, one), VF, 4);
	__m512 az = _mm512_mask_i32gather_ps(zf, all, _mm512_add_epi32(C3, two), VF, 4);
	const __m512 px = _mm512_mask_i32gather_ps(zf, all, C3, P, 4);
	const __m512 py = _mm512_mask_i32gather_ps(zf, all, _mm512_add_epi32(C3, one), P, 4);
	const __m512 pz = _mm512_mask_i32gather_ps(zf, all, _mm512_add_epi32(C3, two), P, 4);

	static const int nb[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
	for(int k = 0; k<8; k++)
	{
		const int xd = nb[k][0], yd = nb[k][1];
		__m512i CN = _mm512_add_epi32(C, _mm512_set1_epi32(xd*SHEET_SIZE+yd));
		// active[x+xd][y+yd], a bool, read as the low byte of a 32 bit gather
		__m512i a = _mm512_and_si512(_mm512_mask_i32gather_epi32(zi, all, CN, ACT, 1), _mm512_set1_epi32(0xFF));
		__mmask16 na = _mm512_mask_cmpneq_epi32_mask(all, a, zi);
		if (!na) continue;
		__m512i CN3 = _mm512_mullo_epi32(CN, three);
		__m512 nx = _mm512_mask_i32gather_ps(zf, na, CN3, P, 4);
		__m512 ny = _mm512_mask_i32gather_ps(zf, na, _mm512_add_epi32(CN3, one), P, 4);
		__m512 nz = _mm512_mask_i32gather_ps(zf, na, _mm512_add_epi32(CN3, two), P, 4);
		__m512 dx = _mm512_sub_ps(px, nx), dy = _mm512_sub_ps(py, ny), dz = _mm512_sub_ps(pz, nz);
		__m512 d2 = _mm512_add_ps(_mm512_add_ps(_mm512_mul_ps(dx, dx), _mm512_mul_ps(dy, dy)), _mm512_mul_ps(dz, dz));
		__m512 d = _mm512_sqrt_ps(d2);
		dx = _mm512_div_ps(dx, d); dy = _mm512_div_ps(dy, d); dz = _mm512_div_ps(dz, d);
		__m512 f = _mm512_sub_ps(_mm512_set1_ps(expectedDistanceLookup[xd+1][yd+1]), d);
		ax = _mm512_mask_add_ps(ax, na, ax, _mm512_mul_ps(dx, f));
		ay = _mm512_mask_add_ps(ay, na, ay, _mm512_mul_ps(dy, f));
		az = _mm512_mask_add_ps(az, na, az, _mm512_mul_ps(dz, f));
	}

	const __m512 spring = _mm512_set1_ps((float)SPRING_FORCE_CONSTANT);
	ax = _mm512_mul_ps(ax, spring); ay = _mm512_mul_ps(ay, spring); az = _mm512_mul_ps(az, spring);

	alignas(64) float fm[16];
	_mm512_store_ps(fm, _mm512_add_ps(_mm512_add_ps(_mm512_mul_ps(ax, ax), _mm512_mul_ps(ay, ay)), _mm512_mul_ps(az, az)));
	for(int l = 0; l<n; l++)
		maxForce = std::max(fm[l], maxForce);

	const __m512 fric = _mm512_set1_ps((float)FRICTION_CONSTANT);
	__m512 vx = _mm512_mask_i32gather_ps(zf, all, C3, V, 4);
	__m512 vy = _mm512_mask_i32gather_ps(zf, all, _mm512_add_epi32(C3, one), V, 4);
	__m512 vz = _mm512_mask_i32gather_ps(zf, all, _mm512_add_epi32(C3, two), V, 4);
	vx = _mm512_add_ps(_mm512_mul_ps(vx, fric), ax);
	vy = _mm512_add_ps(_mm512_mul_ps(vy, fric), ay);
	vz = _mm512_add_ps(_mm512_mul_ps(vz, fric), az);
	_mm512_mask_i32scatter_ps(V, all, C3, vx, 4);
	_mm512_mask_i32scatter_ps(V, all, _mm512_add_epi32(C3, one), vy, 4);
	_mm512_mask_i32scatter_ps(V, all, _mm512_add_epi32(C3, two), vz, 4);
}

// The forces of activeList[lo..hi), in ai order: sixteen at a time when the CPU has AVX-512F,
// else the scalar body one point at a time.
void PatchGenerator::ForcesOnPoints(int lo, int hi, float &maxForce)
{
	if (VectorFieldCalculator::HaveAvx512())
	{
		for(int ai = lo; ai < hi; ai += 16)
			ForcesOnPoint16(ai, hi-ai < 16 ? hi-ai : 16, maxForce);
	}
	else
		for(int ai = lo; ai < hi; ai++)
			ForcesOnPoint(ai, maxForce);
}

// Moves one point and records whether it crossed a voxel boundary. SetVectorField is NOT called
// here: it writes to the shared map vectorFieldLookup and stays in a serial pass.
void PatchGenerator::MovePoint(int ai)
{
	int x = activeList[ai][0], y = activeList[ai][1];

	point oldPos = paperSheet[x][y].pos;
	paperSheet[x][y].pos += velSheet[x][y];

	vfToRedo[ai] = (int(oldPos.x) != int(paperSheet[x][y].pos.x) || int(oldPos.y) != int(paperSheet[x][y].pos.y) || int(oldPos.z) != int(paperSheet[x][y].pos.z));
}

// One relaxation step: forces on every active point, then the move, then the field for the
// points that changed voxel.
float PatchGenerator::ForcesAndMove(void)
{
  float largestForce = 0.0;

  double t0 = now();
  mine().n_relax_points += activeListSize;
  if (pool && pool->nw > 1 && activeListSize >= forcePoolThreshold())
  {
    mine().n_relax_par++;
    // The same two loops, over the same points, in the same per-point code, divided into
    // contiguous blocks. The maxima are combined in worker order, and max is exact.
    pool->run(activeListSize);
    for(int w = 0; w < pool->nw; w++)
      largestForce = std::max(pool->partMax[w], largestForce);
  }
  else
  {
    mine().n_relax_ser++;
    ForcesOnPoints(0, activeListSize, largestForce);

    for(int ai = 0; ai<activeListSize; ai++)
      MovePoint(ai);
  }
  double t1 = now(); mine().t_forces += t1-t0;

  for(int ai = 0; ai<activeListSize; ai++)
    if (vfToRedo[ai])
      SetVectorField(activeList[ai][0],activeList[ai][1]);
  mine().t_setvf += now()-t1;

  return sqrt(largestForce);
}

// The relaxation loop with the parallel region LIFTED out of it.
//
// Why it was tried and why it went. The first attempt (2026-09-11) put the directives inside
// ForcesAndMove, which is called up to 12500 times per patch: about 25000 parallel regions were
// opened, and the result was slower in every configuration tried (4 threads 31 s against 23,
// 8 threads 55 s, 16 threads 454 s). Lifting the region out helped but was still not enough.
//
// Why it stays identical bit for bit. The first loop is a Jacobi update: it reads .pos
// of the neighbours and writes only .vel of its own point. The reduction is over a maximum,
// which is exact and does not depend on the order. The second loop updates .pos, which no other
// point reads in that pass. SetVectorField writes to the shared map and stays serial, in the
// original order.
// The relaxation loop. A parallel variant used to live here, opening one OpenMP region per
// growth step rather than per iteration. It was removed on 2026-09-13 after measurement: its
// output was identical bit for bit, and it was 7.1 times slower at eight threads, because
// raising the thread count multiplies how many regions are opened and not how much work happens
// inside each one.
int PatchGenerator::RelaxToConvergence(float &finalForce)
{
	int j = 0;
	float f = 0.0f;
	while (((f=ForcesAndMove())>RELAX_FORCE_THRESHHOLD && j<MAX_RELAX_ITERATIONS) || j<MIN_RELAX_ITERATIONS)
		j++;
	finalForce = f;
	return j;
}

bool PatchGenerator::TryToFill(int xp, int yp, Vec3 &rp)
{
  if (!active[xp][yp])
  {
    for(int i = 0; i<4; i++)
	{
      int xd = dirVectorLookup[i][0];
	  int yd = dirVectorLookup[i][1];
	  
      int xo1 = xp+xd;
	  int xo2 = xp+xd+xd;
	  int yo1 = yp+yd;
	  int yo2 = yp+yd+yd;

	  // corner point (RH)
      int xc1 = xp+xd-yd;
	  int yc1 = yp+xd+yd;
      // corner point the other side of xo1,yo1	(LH)  
      int xd1 = xp+xd+yd;
	  int yd1 = yp+yd-xd;

	  // right angles to xo1,yo1 (going right)
      int xa1 = xp-yd;
	  int ya1 = yp+xd;
/*
      if (active[xo1][yo1])
	  {
		  printf("%d,%d has active neighbour %d %d\n",xp,yp,xo1,yo1);
		  printf("%d %d status = %s\n",xo2,yo2,active[xo2][yo2]?"active":"inactive");
		  printf("%d %d status = %s\n",xc1,yc1,active[xc1][yc1]?"active":"inactive");
		  printf("%d %d status = %s\n",xd1,yd1,active[xd1][yd1]?"active":"inactive");
		  printf("%d %d status = %s\n",xa1,ya1,active[xa1][ya1]?"active":"inactive");
	  }
*/	  
      // If xo2,yo2 is in bound, then no need to check xo1,yo1
	  if (xo2>=0 && xo2<SHEET_SIZE && yo2>=0 && yo2<SHEET_SIZE && active[xo1][yo1] && active[xo2][yo2] && xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && xd1>=0 && xd1<SHEET_SIZE && yd1>=0 && yd1<SHEET_SIZE && active[xc1][yc1] && active[xd1][yd1])
	  {
		// Adjacent point, point beyond that, and points either side of adjacent point are all active
		rp = 2*paperSheet[xo1][yo1].pos - paperSheet[xo2][yo2].pos;
		//printf("Straightfill %f %f %f\n",rp.x,rp.y,rp.z);
		return true;
	  }
      // If xc1,yc1 in bounds then no need to check xa1,ya1 or xo1,yo1
      if (xc1>=0 && xc1<SHEET_SIZE && yc1>=0 && yc1<SHEET_SIZE && active[xo1][yo1] && active[xa1][ya1] && active[xc1][yc1])
	  {
          // Get midpoint of xo1,yo1,xa1,ya1
		  rp = (paperSheet[xo1][yo1].pos+paperSheet[xa1][ya1].pos)/2;
          // Extend from the corner through the midpoint
          rp = 2*rp - paperSheet[xc1][yc1].pos;
		  //printf("Cornerfill %f %f %f\n",rp.x,rp.y,rp.z);
		  return true;
	  }
	}
  }
  return false;
}

// Returns the page holding block (zb,yb,xb), allocating it when needed and when 'creating'.
// Out of bounds it returns NULL: in the full array an index out of range read somebody else's
// memory, which is undefined behaviour and not a property worth preserving.
unsigned char *PatchGenerator::StressPage(int zb, int yb, int xb, bool creating)
{
	if (zb < 0 || yb < 0 || xb < 0 || zb >= STRESS_NZ || yb >= STRESS_NY || xb >= STRESS_NX)
		return NULL;
	unsigned char *&pg = stressPage[zb/STRESS_PAGE][yb/STRESS_PAGE][xb/STRESS_PAGE];
	if (!pg && creating)
		pg = (unsigned char *)calloc((size_t)STRESS_PAGE*STRESS_PAGE*STRESS_PAGE, 1);
	return pg;
}

void PatchGenerator::ClearHighStress(void)
{
	// Freeing the pages is both the clear and the release. The original walked 206 MiB with the
	// loops inverted with respect to the layout (x outermost, z innermost on a [z][y][x] array):
	// one cache line and one TLB entry for every single byte.
	for(int a = 0; a<STRESS_PZ; a++)
	for(int b = 0; b<STRESS_PY; b++)
	for(int c = 0; c<STRESS_PX; c++)
		if (stressPage[a][b][c]) { free(stressPage[a][b][c]); stressPage[a][b][c] = NULL; }
}

bool PatchGenerator::MarkHighStress(void)
{  
  bool r = false;
  
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
	{
	  float SETot = 0;
	  int SENum = 0;
	  // Don't bother with range check - make sure elsewhere that edges of sheet are not approached
      for(int xo=x-1; xo<x+2; xo++)
      for(int yo=y-1; yo<y+2; yo++)
        if (active[xo][yo] && ! (xo == x && yo == y)) 
		{
            float expectedDistance = expectedDistanceLookup[x-xo+1][y-yo+1];
            Vec3 direction = paperSheet[x][y].pos-paperSheet[xo][yo].pos;
            float actualDistance = direction.length();
                
            float force = (expectedDistance - actualDistance);
            SETot += force*force;
			SENum += 1;
		}
		
	  if (SETot/SENum > HIGH_STRESS_THRESHHOLD)
	  {
	    // paperPos is in x,y,z order
		int xb = (int)((paperSheet[x][y].pos.x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE);
		int yb = (int)((paperSheet[x][y].pos.y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE);
		int zb = (int)((paperSheet[x][y].pos.z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE);
		
		// The same count the table is sized with, so that the neighbour of a block can be the
		// last one along an axis. Spelled VOL_SIZE_X/STRESS_BLOCK_SIZE here, this loop stopped
		// one block short of the table even after the table was made to cover the volume.
		for(int xo = xb-(xb>0); xo <= xb+(xb+1<STRESS_NX); xo++)
		for(int yo = yb-(yb>0); yo <= yb+(yb+1<STRESS_NY); yo++)
		for(int zo = zb-(zb>0); zo <= zb+(zb+1<STRESS_NZ); zo++)
 		{
			unsigned char *pg = StressPage(zo,yo,xo,true);
			if (pg)
			{
				pg[((zo%STRESS_PAGE)*STRESS_PAGE + (yo%STRESS_PAGE))*STRESS_PAGE + (xo%STRESS_PAGE)] = 1;
				r = true;
			}
		}
		
		if (!silent) printf("\nHigh stress at x,y,z=%f,%f,%f",paperSheet[x][y].pos.x,paperSheet[x][y].pos.y,paperSheet[x][y].pos.z);
	  }
	}
	
	return r;
}

bool PatchGenerator::HasHighStress(int x, int y, int z)
{
	int zb = (z-VOL_OFFSET_Z)/STRESS_BLOCK_SIZE;
	int yb = (y-VOL_OFFSET_Y)/STRESS_BLOCK_SIZE;
	int xb = (x-VOL_OFFSET_X)/STRESS_BLOCK_SIZE;
	unsigned char *pg = StressPage(zb,yb,xb,false);
	if (!pg) return false;
	return pg[((zb%STRESS_PAGE)*STRESS_PAGE + (yb%STRESS_PAGE))*STRESS_PAGE + (xb%STRESS_PAGE)] != 0;
}

// TODO newPtsPaper needs to be vector of int vec2
int PatchGenerator::MakeNewPoints(pointSet &newPts, pointSet &newPtsPaper)
{
    Vec3 np;

	ClearPointLookup();
    for(int x = 0; x<SHEET_SIZE; x++)
    for(int y = 0; y<SHEET_SIZE; y++)
    {
		if (active[x][y])
			AddPointToLookup(point(paperSheet[x][y].pos));
	}
	std::set<pair<int,int>> inactiveNeighbours;
    for(int ai = 0; ai<activeListSize; ai++)
    {
      int xa = activeList[ai][0], ya = activeList[ai][1];
	  int offsets[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
	  for(int oi = 0; oi<4; oi++)
	  {
		  int x = xa+offsets[oi][0];
		  int y = ya+offsets[oi][1];
		  
		  if (!active[x][y]) inactiveNeighbours.insert(pair<int,int>(x,y));
      }
	}
	for(auto &p : inactiveNeighbours)
	{
		int x = p.first, y = p.second;
		  if (TryToFill(x,y,np))
		  { 
			float dist;
			bool okayToFill = true;
			int px = (int)np.x;
			int py = (int)np.y;
			int pz = (int)np.z;
			if (px-VOL_OFFSET_X>=MARGIN && px-VOL_OFFSET_X<VOL_SIZE_X-MARGIN && py-VOL_OFFSET_Y>=MARGIN && py-VOL_OFFSET_Y<VOL_SIZE_Y-MARGIN && pz-VOL_OFFSET_Z>=MARGIN && pz-VOL_OFFSET_Z<VOL_SIZE_Z-MARGIN)
			{
			  if (HasHighStress(px,py,pz))
			  {
				  //printf("1");
				  okayToFill = false;
			  }
			  else if ((dist=GetDistanceAtPoint(px,py,pz))>0)
			  {
				  //printf("2:%f:",dist);
				  okayToFill = false;
			  }
			  else if (HasCloseNeighbour(np))
			  {
				  //printf("3");
				  okayToFill = false;
			  }
			  
			  if (okayToFill)
			  {
				//printf(":New %d,%d:",x,y);  
				newPtsPaper.push_back(point(x,y,0.0));
				newPts.push_back(np);
			  }
			}
			else
			{
				//printf("out of bounds at %d,%d,%d\n",px,py,pz);  
			}
		  }
      
    }
    //printf("%d points added\n",(int)newPts.size());     

    return newPts.size();	
}

void PatchGenerator::AddNewPoints(pointSet &newPts, pointSet &newPtsPaper)
{
    // Two passes instead of one. Positions and activations are settled first, and the field is
    // computed afterwards, when every new point is already in place.
    for(int i = 0; i<(int)newPts.size(); i++)
    {
	  point paperPoint = newPtsPaper[i];
	  int x = paperPoint.x;
	  int y = paperPoint.y;
	  paperSheet[x][y].pos = newPts[i];

	  velSheet[x][y].x = 0.0;
	  velSheet[x][y].y = 0.0;
	  velSheet[x][y].z = 0.0;

      MakeActive(x,y);
    }

    // The field of the new points is computed here, in a pass of its own: by now they are all
    // active and their positions are settled.
    for(int i = 0; i<(int)newPts.size(); i++)
      SetVectorField((int)newPtsPaper[i].x,(int)newPtsPaper[i].y);
}

PatchGenerator::PatchGenerator(const string &surfaceZarrName_) : surfaceZarrName(surfaceZarrName_), vfc(NULL)
{
	if (!silent) printf("Init\n");

    InitExpectedDistanceLookup();
	
    activeListSize = 0;	
    surfaceZarr = NULL;   // opened lazily on the first patch, then kept open
    std::memset(stressPage, 0, sizeof(stressPage));

    // One pool per slot, for the life of the run. With SIMPAPER_FORCE_THREADS=1, which is the
    // default, no thread is created and the relaxation loop is the serial one.
    if (forcePoolThreads() > 1)
    {
        pool = new ForcePool();
        pool->start(this, forcePoolThreads());
    }
}

		
PatchGenerator::~PatchGenerator(void)
{
	// The zarr reader and the vector field calculator stay alive from one patch to the next:
	// they are released here.
	if (pool) { pool->stop(); delete pool; pool = NULL; }
	if (vfc) { delete vfc; vfc = NULL; }
	if (surfaceZarr) { ZARRClose_1(surfaceZarr); surfaceZarr = NULL; }
	ClearHighStress();
}

bool PatchGenerator::SetSeed(const std::vector<float> &seed)
{  
    if (ZARRRead_1(surfaceZarr,seed[2],seed[1],seed[0]) != 255)
	  return false;

    for(int x=0; x<SHEET_SIZE; x++)
    for(int y=0; y<SHEET_SIZE; y++)
    {
        active[x][y] = false;
    }
  
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.x = seed[0];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.y = seed[1];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2].pos.z = seed[2];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2);
  
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.x = seed[0]+QUADMESH_SIZE*seed[3];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.y = seed[1]+QUADMESH_SIZE*seed[4];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2+1].pos.z = seed[2]+QUADMESH_SIZE*seed[5];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2+1);

    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.x = seed[0]-QUADMESH_SIZE*seed[3];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.y = seed[1]-QUADMESH_SIZE*seed[4];
    paperSheet[SHEET_SIZE/2][SHEET_SIZE/2-1].pos.z = seed[2]-QUADMESH_SIZE*seed[5];
    SetVectorField(SHEET_SIZE/2,SHEET_SIZE/2-1);

    paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.x = seed[0]+QUADMESH_SIZE*seed[6];
    paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.y = seed[1]+QUADMESH_SIZE*seed[7];
    paperSheet[SHEET_SIZE/2+1][SHEET_SIZE/2].pos.z = seed[2]+QUADMESH_SIZE*seed[8];
    SetVectorField(SHEET_SIZE/2+1,SHEET_SIZE/2);

    paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.x = seed[0]-QUADMESH_SIZE*seed[6];
    paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.y = seed[1]-QUADMESH_SIZE*seed[7];
    paperSheet[SHEET_SIZE/2-1][SHEET_SIZE/2].pos.z = seed[2]-QUADMESH_SIZE*seed[8];
    SetVectorField(SHEET_SIZE/2-1,SHEET_SIZE/2);

    MakeActive(SHEET_SIZE/2,SHEET_SIZE/2);
    MakeActive(SHEET_SIZE/2,SHEET_SIZE/2+1);
    MakeActive(SHEET_SIZE/2,SHEET_SIZE/2-1);
    MakeActive(SHEET_SIZE/2+1,SHEET_SIZE/2);
    MakeActive(SHEET_SIZE/2-1,SHEET_SIZE/2);
    
    return true;
}


void PatchGenerator::OutputBoundary(Patch &boundary, pointSet &boundaryPoints, pointSet &boundaryPointsPaper)
{
  vector<patchPoint> points;

  for(unsigned i = 0; i<boundaryPoints.size(); i++)
  {
	points.push_back(patchPoint(
		boundaryPointsPaper[i].x-SHEET_SIZE/2,
		boundaryPointsPaper[i].y-SHEET_SIZE/2,
		boundaryPoints[i].x,
		boundaryPoints[i].y,
		boundaryPoints[i].z));
  }
  
  boundary.BuildFromPoints(points,0);
}

void PatchGenerator::OutputPatch(Patch &patch, int iter)
{ 
  vector<patchPoint> points;
 
  for(int x = 0; x<SHEET_SIZE; x++)
  for(int y = 0; y<SHEET_SIZE; y++)
	if (active[x][y])
    {
		points.push_back(patchPoint(x-SHEET_SIZE/2,
		                           y-SHEET_SIZE/2,
		                           paperSheet[x][y].pos.x,
		                           paperSheet[x][y].pos.y,
								   paperSheet[x][y].pos.z));
	}
	
  if (points.size()==0)
  {
	  printf("Error - PatchGenerator::OutputPatch has encountered points.size()==0\n");
  }
  else
  {
	  if (!silent) printf("PatchGenerator::OutputPatch generated %d points\n",(int)points.size());
  }	  
  
  // Debug individual patch problem
/*  if (iter==347)
  {
	  FILE *f = fopen("d:/pipelineOutput/debug.csv","w");
	  for(auto &p : points)
	  {
		  fprintf(f,"%f,%f %f,%f,%f\n",p.x,p.y,p.v.x,p.v.y,p.v.z);
	  }
	  fclose(f);
	  exit(-1);
  }
  */
  patch.BuildFromPoints(points,iter);
  
  patch.SetPatchNum(iter);
}

int PatchGenerator::GeneratePatch(const std::vector<float> &seed,Patch &patch, Patch &boundary, int iter, bool _silent)
{
  double t_patch = now(); 
  silent = _silent;
  if (!silent) printf("Called GeneratePatch\n");
  int totPointsAdded = 0;
 
  activeListSize = 0;
  
  ClearHighStress();
  ClearPointLookup();
  distanceLookup.clear();

  // The cache of smoothed field values, keyed by voxel, was cleared once every thousand patches.
  // Over a run it therefore grows to hold every voxel every patch of this slot has visited, and it
  // is consulted about two hundred million times, at addresses with no locality. Seeds are kept
  // 600 voxels apart, so what a previous patch left in it is almost never asked for again: the
  // entries are dead weight that pushes the live ones out of cache.
  // Clearing it for every patch cannot change a value, because what it holds is a function of the
  // voxel and the field and is recomputed identically when it is not there.
  // SIMPAPER_FIELD_CACHE_EVERY sets the period; 1 is per patch, 0 restores the old 1000.
  {
    static int period = -1;
    if (period < 0)
    {
      const char *e = getenv("SIMPAPER_FIELD_CACHE_EVERY");
      period = e ? atoi(e) : 1;
      if (period <= 0) period = 1000;
    }
    if (iter%period==0)
        vectorFieldLookup.clear();
  }
  
  pointSet newPtsPaper;
  pointSet newPts;

  // The author's TODO that used to sit here ("in future it would be better to keep the zarrs open")
  // is applied: the reader is opened once and lives as long as the generator, instead of being
  // allocated and destroyed for every patch. ZARR_1 holds 80 decompressed 192^3 buffers, that is
  // 540 MiB: opening and closing it fifteen hundred times per run meant throwing that cache away
  // and rebuilding it every time, with four threads doing it at once.
  // This is safe because during growth the zarr is read-only: there is no ZARRWrite_1 anywhere on
  // this path, so no eviction ever writes to disk and a chunk read again gives the same bytes.
  // It is closed in the destructor.
  if (!surfaceZarr)
  {
    if (!silent) printf("Opening zarrs\n");
    surfaceZarr = ZARROpen_1(surfaceZarrName.c_str());
  }

  // TRIED on 2026-09-11, keeping it alive across patches like the zarr reader: it does NOT pay,
  // and it costs 577 MB. The reason is MIN_SEED_DISTANCE, which forces 600 voxels between seeds,
  // so two patches never overlap and there is nothing to reuse. Within one patch, instead, the
  // cache most certainly does pay, and so it stays.
  vfc = new VectorFieldCalculator(surfaceZarr);
  
  if (!SetSeed(seed))
  {
	  printf("Seed point is not a surface point\n");
	  delete vfc; vfc = NULL;
	  return 0;
  }
  
  int i;
  int totIters=0;
  float f;  
  for(i = 0; i<MAX_GROWTH_STEPS; i++)
  {
    if (!silent) printf("#");
	fflush(stdout);
    int j = RelaxToConvergence(f);
	totIters += j;
	
	double ts0 = now();
	bool stress_found = MarkHighStress();
	mine().t_stress += now()-ts0;
	if (stress_found)
	{
	  if (!silent) printf("\nHigh stress encountered\n");
	  break;
	}
	
    newPts.clear();
    newPtsPaper.clear();
	double tn0 = now();
	totPointsAdded += MakeNewPoints(newPts,newPtsPaper);
	double tn1 = now(); mine().t_makepts += tn1-tn0;
	AddNewPoints(newPts,newPtsPaper);
	mine().t_addpts += now()-tn1;
	mine().t_newpts += now()-tn0;
	
    if (totPointsAdded <10 && i==10)
	{
		if (!silent) printf("\nAborting, too few points added");
		break;
	}
  }
 
  if (!silent)
  { 
    printf("\n");
    printf("Growth steps:%d\n",i);
    printf("Mean relaxation iterations:%f\n",((float)totIters)/(float)i);
  }
  
  OutputPatch(patch,iter);
  
  if (newPts.size()>0)
	OutputBoundary(boundary,newPts,newPtsPaper);

  delete vfc; vfc = NULL;
  mine().t_total += now()-t_patch;

  return i;
}

