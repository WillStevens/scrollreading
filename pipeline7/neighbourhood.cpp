#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <tuple>
#include <stdexcept>

// BFS from a seed vertex, collecting all vertices within max_dist hops.
std::unordered_set<int> bfs(
    int seed,
    int max_dist,
    const std::unordered_map<int, std::vector<int>>& adj)
{
    std::unordered_set<int> visited;
    std::queue<std::pair<int,int>> q;  // (vertex, distance)

    q.push({seed, 0});
    visited.insert(seed);

    while (!q.empty()) {
        auto [v, d] = q.front();
        q.pop();

        if (d >= max_dist) continue;

        auto it = adj.find(v);
        if (it == adj.end()) continue;

        for (int nb : it->second) {
            if (!visited.count(nb)) {
                visited.insert(nb);
                q.push({nb, d + 1});
            }
        }
    }
    return visited;
}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> <vertex1> <vertex2> <distance>\n";
        return 1;
    }

    const std::string filename = argv[1];
    int v1, v2, max_dist;
    try {
        v1       = std::stoi(argv[2]);
        v2       = std::stoi(argv[3]);
        max_dist = std::stoi(argv[4]);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << "\n";
        return 1;
    }

    if (max_dist < 0) {
        std::cerr << "Distance must be >= 0\n";
        return 1;
    }

    // --- Load CSV ---
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Cannot open file: " << filename << "\n";
        return 1;
    }

    // Store each original row so we can reproduce it exactly on output.
    struct Edge {
        int p1, p2;
        std::string line;  // full original CSV line
    };

    std::vector<Edge> edges;
    std::unordered_map<int, std::vector<int>> adj;

    std::string row;
    while (std::getline(infile, row)) {
        // Skip empty lines / header lines that don't start with a digit
        if (row.empty()) continue;
        if (!std::isdigit(static_cast<unsigned char>(row[0])) && row[0] != '-')
            continue;

        std::istringstream ss(row);
        std::string tok1, tok2;
        if (!std::getline(ss, tok1, ',') || !std::getline(ss, tok2, ','))
            continue;

        try {
            int p1 = std::stoi(tok1);
            int p2 = std::stoi(tok2);
            edges.push_back({p1, p2, row});
            adj[p1].push_back(p2);
            adj[p2].push_back(p1);
        } catch (...) {
            // Skip rows that can't be parsed
        }
    }
    infile.close();

    // --- BFS from each seed vertex ---
    std::unordered_set<int> reachable1 = bfs(v1, max_dist, adj);
    std::unordered_set<int> reachable2 = bfs(v2, max_dist, adj);

    // Union of both reachable sets
    std::unordered_set<int> keep = reachable1;
    keep.insert(reachable2.begin(), reachable2.end());

    // --- Output edges where BOTH endpoints are in the keep set ---
    // This faithfully reproduces the subgraph: an edge is included only when
    // both of its vertices lie within distance of at least one seed.
    for (const auto& e : edges) {
        if (keep.count(e.p1) && keep.count(e.p2)) {
            std::cout << e.line << "\n";
        }
    }

    return 0;
}