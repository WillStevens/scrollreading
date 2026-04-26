/*
 * shortest_path.cpp
 *
 * Finds the shortest (lowest total mismatch) path between two patches
 * in a weighted undirected graph defined by a CSV file.
 *
 * Edges with mismatch > threshold are ignored entirely.
 * If a vertex list file is supplied, only edges where BOTH endpoints appear
 * in that list are considered, constraining the path to those vertices.
 *
 * Build:  g++ -O2 -std=c++17 -o shortest_path shortest_path.cpp
 * Usage:  shortest_path <csv_file> <patch1> <patch2> <threshold> [vertex_file]
 *
 *   csv_file     CSV with rows: patch1,patch2,mismatch
 *   patch1       source vertex ID
 *   patch2       target vertex ID
 *   threshold    edges with mismatch > threshold are ignored
 *   vertex_file  optional file with one vertex ID per line;
 *                the path is restricted to these vertices
 *
 * Output: comma-separated list of vertex numbers from patch1 to patch2,
 *         e.g.  3,17,42,8
 *         followed by the total path cost on the next line.
 */

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

using VertexId = int;
using Cost     = double;

struct Edge {
    VertexId to;
    Cost     weight;
};

using Graph   = std::map<VertexId, std::vector<Edge>>;
using PQEntry = std::pair<Cost, VertexId>;

static const Cost INF = std::numeric_limits<Cost>::infinity();

// ---------------------------------------------------------------------------
// Load allowed vertex set from a single-column file (one vertex ID per line).
// Returns an empty set if filename is empty (meaning: no filter).
// ---------------------------------------------------------------------------
static std::set<VertexId> loadVertexList(const std::string& filename)
{
    std::set<VertexId> allowed;
    if (filename.empty()) return allowed;

    std::ifstream f(filename);
    if (!f) {
        std::fprintf(stderr, "Cannot open vertex file: %s\n", filename.c_str());
        std::exit(1);
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        try {
            allowed.insert(std::stoi(line));
        } catch (...) {
            continue;   // skip header / non-numeric lines
        }
    }

    std::printf("Vertex list loaded: %zu vertices.\n", allowed.size());
    return allowed;
}

// ---------------------------------------------------------------------------
// Load graph from CSV.
// Discards edges above threshold.
// If allowedVertices is non-empty, also discards edges where either
// endpoint is not in the allowed set.
// ---------------------------------------------------------------------------
static Graph loadGraph(const std::string& filename, Cost threshold,
                       const std::set<VertexId>& allowedVertices)
{
    const bool filterVertices = !allowedVertices.empty();
    Graph graph;

    std::ifstream f(filename);
    if (!f) {
        std::fprintf(stderr, "Cannot open file: %s\n", filename.c_str());
        std::exit(1);
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(f, line)) {
        ++lineNum;
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream ss(line);
        std::string tok1, tok2, tok3;
        if (!std::getline(ss, tok1, ',') ||
            !std::getline(ss, tok2, ',') ||
            !std::getline(ss, tok3, ',')) {
            std::fprintf(stderr, "Warning: skipping malformed line %d: %s\n",
                         lineNum, line.c_str());
            continue;
        }

        VertexId p1, p2;
        Cost     mismatch;
        try {
            p1       = std::stoi(tok1);
            p2       = std::stoi(tok2);
            mismatch = std::stod(tok3);
        } catch (...) {
            continue;   // skip header or non-numeric lines
        }

        if (mismatch > threshold) continue;

        if (filterVertices &&
            (allowedVertices.find(p1) == allowedVertices.end() ||
             allowedVertices.find(p2) == allowedVertices.end())) continue;

        // Undirected: add both directions
        graph[p1].push_back({p2, mismatch});
        graph[p2].push_back({p1, mismatch});
        graph[p1];  // ensure vertex exists even with no outgoing edges
        graph[p2];
    }

    return graph;
}

// ---------------------------------------------------------------------------
// Dijkstra's algorithm
// Returns the path as a vertex list, or empty if no path exists.
// ---------------------------------------------------------------------------
static std::vector<VertexId> dijkstra(
    const Graph& graph,
    VertexId     source,
    VertexId     target,
    Cost&        totalCost)
{
    std::map<VertexId, Cost>     dist;
    std::map<VertexId, VertexId> prev;

    for (const auto& [v, _] : graph) dist[v] = INF;

    if (dist.find(source) == dist.end()) {
        std::fprintf(stderr, "Source vertex %d not found in graph.\n", source);
        std::exit(1);
    }
    if (dist.find(target) == dist.end()) {
        std::fprintf(stderr, "Target vertex %d not found in graph.\n", target);
        std::exit(1);
    }

    dist[source] = 0.0;

    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;
    pq.push({0.0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) continue;   // stale entry
        if (u == target) break;

        auto it = graph.find(u);
        if (it == graph.end()) continue;

        for (const Edge& e : it->second) {
            Cost nd = dist[u] + e.weight;
            if (nd < dist[e.to]) {
                dist[e.to] = nd;
                prev[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }

    totalCost = dist[target];
    if (totalCost == INF) return {};

    std::vector<VertexId> path;
    for (VertexId v = target; v != source; v = prev[v]) {
        path.push_back(v);
        if (prev.find(v) == prev.end()) return {};
    }
    path.push_back(source);
    std::reverse(path.begin(), path.end());
    return path;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc < 5 || argc > 6) {
        std::fprintf(stderr,
            "Usage: shortest_path <csv_file> <patch1> <patch2> <threshold> [vertex_file]\n"
            "  csv_file     CSV with rows: patch1,patch2,mismatch\n"
            "  patch1       source vertex ID\n"
            "  patch2       target vertex ID\n"
            "  threshold    edges with mismatch > threshold are ignored\n"
            "  vertex_file  optional file with one vertex ID per line;\n"
            "               the path is restricted to these vertices\n");
        return 1;
    }

    std::string filename   = argv[1];
    VertexId    source     = std::stoi(argv[2]);
    VertexId    target     = std::stoi(argv[3]);
    Cost        threshold  = std::stod(argv[4]);
    std::string vertexFile = (argc == 6) ? argv[5] : "";

    std::set<VertexId> allowedVertices = loadVertexList(vertexFile);
    Graph graph = loadGraph(filename, threshold, allowedVertices);

    std::printf("Graph loaded: %zu vertices.\n", graph.size());

    Cost totalCost = 0.0;
    std::vector<VertexId> path = dijkstra(graph, source, target, totalCost);

    if (path.empty()) {
        std::printf("No path found from %d to %d "
                    "(try raising the threshold or check the vertex list).\n",
                    source, target);
        return 1;
    }

    for (int i = 0; i < (int)path.size(); ++i) {
        if (i > 0) std::putchar(',');
        std::printf("%d", path[i]);
    }
    std::putchar('\n');
    std::printf("Total cost: %g  (%zu hops)\n", totalCost, path.size() - 1);

    return 0;
}
