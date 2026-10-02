#pragma once

// CellGrid: a flat spatial hash from an integer cell (x,y,z) to the points that fell in it.
//
// It replaces std::map< std::tuple<int,int,int>, std::vector<P> > where the map is filled once from a
// vector of points and then only looked up or iterated. What the map guaranteed is kept:
//   - the points of a cell come out in the order they were added (the map's vector push_back order);
//   - SortedCells() lists the cells in the order the map iterated them: x, then y, then z, signed,
//     which is std::less<std::tuple<int,int,int>>.
// What changes: one open addressing table of cell ids, one array of keys, one array of all the points
// grouped by cell (a stable counting sort), instead of one tree node and one heap vector per cell.

#include <stdint.h>
#include <algorithm>
#include <vector>

template <class P>
class CellGrid
{
	public:
		struct Span
		{
			const P *b, *e;
			const P *begin() const { return b; }
			const P *end() const { return e; }
		};

		// key(point, x, y, z) writes the cell of a point
		template <class KeyFn>
		void Build(const std::vector<P> &pts, KeyFn key)
		{
			size_t n = pts.size();
			kx.clear(); ky.clear(); kz.clear();
			size_t cap = 64;
			while (cap < n) cap <<= 1;
			table.assign(cap, -1);
			mask = (uint32_t)(cap - 1);
			cellOf.resize(n);
			std::vector<uint32_t> count;
			for(size_t i = 0; i < n; i++)
			{
				int x, y, z;
				key(pts[i], x, y, z);
				int id = FindOrInsert(x, y, z);
				if ((size_t)id == count.size())
					count.push_back(0);
				count[id]++;
				cellOf[i] = id;
			}
			size_t cells = kx.size();
			start.assign(cells + 1, 0);
			for(size_t c = 0; c < cells; c++)
				start[c + 1] = start[c] + count[c];
			std::vector<uint32_t> pos(start.begin(), start.end() - 1);
			sorted.resize(n);
			for(size_t i = 0; i < n; i++)
				sorted[pos[cellOf[i]]++] = pts[i];
		}

		// the cell id of (x,y,z), or -1 when no point fell in it
		int Find(int x, int y, int z) const
		{
			uint32_t h = Hash(x, y, z) & mask;
			while (true)
			{
				int id = table[h];
				if (id < 0) return -1;
				if (kx[id] == x && ky[id] == y && kz[id] == z) return id;
				h = (h + 1) & mask;
			}
		}

		Span Cell(int id) const
		{
			Span s;
			s.b = sorted.data() + start[id];
			s.e = sorted.data() + start[id + 1];
			return s;
		}

		int X(int id) const { return kx[id]; }
		int Y(int id) const { return ky[id]; }
		int Z(int id) const { return kz[id]; }
		int NumCells() const { return (int)kx.size(); }

		// every cell id, in the iteration order of std::map<std::tuple<int,int,int>, ...>
		void SortedCells(std::vector<int> &ids) const
		{
			ids.resize(kx.size());
			for(size_t i = 0; i < ids.size(); i++) ids[i] = (int)i;
			std::sort(ids.begin(), ids.end(), [this](int a, int b) {
				if (kx[a] != kx[b]) return kx[a] < kx[b];
				if (ky[a] != ky[b]) return ky[a] < ky[b];
				return kz[a] < kz[b];
			});
		}

	private:
		std::vector<int> kx, ky, kz;
		std::vector<int32_t> table;
		uint32_t mask = 0;
		std::vector<int32_t> cellOf;
		std::vector<uint32_t> start;
		std::vector<P> sorted;

		static uint32_t Hash(int x, int y, int z)
		{
			uint64_t h = (uint64_t)(uint32_t)x * 0x9E3779B97F4A7C15ULL;
			h ^= (uint64_t)(uint32_t)y * 0xC2B2AE3D27D4EB4FULL;
			h ^= (uint64_t)(uint32_t)z * 0x165667B19E3779F9ULL;
			h ^= h >> 31;
			return (uint32_t)(h ^ (h >> 32));
		}

		int FindOrInsert(int x, int y, int z)
		{
			uint32_t h = Hash(x, y, z) & mask;
			while (true)
			{
				int id = table[h];
				if (id < 0) break;
				if (kx[id] == x && ky[id] == y && kz[id] == z) return id;
				h = (h + 1) & mask;
			}
			int id = (int)kx.size();
			kx.push_back(x); ky.push_back(y); kz.push_back(z);
			table[h] = id;
			if (kx.size() * 2 > table.size())
				Grow();
			return id;
		}

		void Grow()
		{
			table.assign(table.size() * 2, -1);
			mask = (uint32_t)(table.size() - 1);
			for(size_t id = 0; id < kx.size(); id++)
			{
				uint32_t h = Hash(kx[id], ky[id], kz[id]) & mask;
				while (table[h] >= 0) h = (h + 1) & mask;
				table[h] = (int32_t)id;
			}
		}
};
