#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

#include "zarr_1.h"
#include "vec3.h"
#include "parameters.h"

// Calculate a single vector field point

#define VF_RAD 2
#define SORTED_DIST_SIZE ((VF_RAD*2+1)*(VF_RAD*2+1)*(VF_RAD*2+1)-1)

#define SMOOTH_WINDOW 2
#define GAUSS_SCALE 1

class VectorFieldCalculator
{
	public:
		VectorFieldCalculator(ZARR_1 *sf);
		~VectorFieldCalculator();
		
		void GetVectorField(int x, int y, int z, Vec3 &v);
		// Pure version: computes and nothing else. It neither reads nor writes the cache, and
		// it reads through the reader it is given, so several voxels can be computed at once,
		// each with its own reader and no shared state.
		void ComputeField(int x, int y, int z, Vec3 &v, ZARR_1 *reader);
		// Computes the field from a dense, immutable snapshot of the region instead of through
		// the zarr reader: no state, no lock, and the same buffer can serve several callers.
		// The arithmetic is identical to ComputeField, operation for operation and in the same
		// order, so the output is identical byte for byte. All the taps one smoothing evaluation
		// needs fit in a 9x9x9 cube, 729 bytes, which sits in L1.
		void ComputeFieldDense(int x, int y, int z, Vec3 &v,
		                       const unsigned char *dense, int oz, int oy, int ox,
		                       int nz, int ny, int nx);
		// ComputeFieldDense for up to 16 taps at once, one AVX-512 lane per tap: lane l computes
		// value[idx[l]] from (cx,cy,cz)[idx[l]] with ComputeFieldDense's operations in its order,
		// the early break of each lane kept by a mask. A lane that the scalar code leaves
		// unassigned keeps the value it held. 'dense' must have 3 readable bytes past its end
		// (the byte gathers read a 32 bit word). Called only when the CPU has AVX-512F.
		void ComputeFieldDense16(const int *cx, const int *cy, const int *cz, Vec3 *value,
		                         const int *idx, int n,
		                         const unsigned char *dense, int oz, int oy, int ox,
		                         int nz, int ny, int nx);
		static bool HaveAvx512(void);

        void GetSmoothedVectorField(int x, int y, int z, Vec3 &v);
		void GetSmoothedVectorFieldInt8(int x, int y, int z, Vec3 &v);
		

		ZARR_1 *surfaceZarr;
		float sortedDistances[SORTED_DIST_SIZE][7];

		// Cache of the field taps. It used to be a std::unordered_map: O(1), but every lookup
		// reads the bucket vector and then CHASES a pointer to a separately allocated node, that
		// is two cold memory accesses, seventy million lookups per run over twenty-three million
		// entries. Here it is a flat open-addressed table: one cache line per lookup, no
		// per-entry allocation, and less memory.
		// Dense paging by coordinate would be worse: the voxels touched form a surface, not a
		// solid volume, so nine pages out of ten would stay empty.
		struct FieldSlot { uint64_t k; Vec3 v; };
		// A slot that holds a whole 4x4x4 block of voxels instead of one.
		// Why: an evaluation of the smoothed field probes the cache 125 times, once per tap of a
		// 5^3 window, and with one voxel per slot those 125 probes land on 125 unrelated cache
		// lines. The 125 taps of a window lie in at most 8 blocks of side 4, so with a block per
		// slot the same 125 probes land in 8 regions of 784 bytes and all but the first few hit
		// L1. Nothing about what the cache returns changes: the key of a voxel is decomposed into
		// (block, offset) by a mapping that is one-to-one, so two voxels share an entry exactly
		// when they shared one before, including the keys the caller packs badly.
		struct FieldBlock { uint64_t k; uint64_t valid; Vec3 v[64]; };
		// The table is allocated by hand rather than by std::vector for one reason: so that it
		// can be asked for huge pages. The run walks it about two hundred million times at
		// addresses that have no locality, and the dTLB profile of the production build puts
		// 87 % of its load misses in this table and in the routine that reads it. A 2 MiB page
		// covers 512 times what a 4 KiB page covers, and nothing else about the table changes:
		// same size, same hash, same probe order, same values, only the addresses differ.
		// SIMPAPER_HUGE_PAGES=0 asks for ordinary pages instead.
		class FieldCache
		{
			FieldBlock *tab = nullptr;
			size_t cap = 0, mask = 0, used = 0;
			void grow();
			static FieldBlock *allocSlots(size_t n);
			static void freeSlots(FieldBlock *p, size_t n);
			// key -> (block key, offset in block). One-to-one, so the equivalence classes of keys
			// are exactly the ones the flat table had.
			static inline void split(uint64_t k, uint64_t &bk, unsigned &off)
			{
				uint64_t x = k >> 32, y = (k >> 16) & 0xFFFFu, z = k & 0xFFFFu;
				bk = (((x >> 2) << 32) | ((y >> 2) << 16) | (z >> 2)) + 1;   // +1: 0 marks empty
				off = (unsigned)(((x & 3) << 4) | ((y & 3) << 2) | (z & 3));
			}
		public:
			FieldCache();
			~FieldCache();
			FieldCache(const FieldCache &) = delete;
			FieldCache &operator=(const FieldCache &) = delete;
			const Vec3 *find(uint64_t k) const;
			void insert(uint64_t k, const Vec3 &v);
			// The probe kept, so a lookup that misses and the insert that follows it walk the
			// table once between them. reserveFor() before the probes keeps the table still.
			const Vec3 *findWithSlot(uint64_t k, size_t &slot) const;
			void insertAt(size_t slot, uint64_t k, const Vec3 &v);
			void reserveFor(size_t extra);
			size_t size() const { return used; }
			void clear();
		};
		FieldCache vectorFieldLookup;

};

