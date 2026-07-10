/*
 * extract_edges.cpp
 *
 * Reads a space-separated file where each row is:
 *   patch1 radius patch2 [ignored fields...]
 *
 * Outputs a CSV file with rows:
 *   patch1,patch2
 *
 * Build:  g++ -O2 -std=c++17 -o extract_edges extract_edges.cpp
 * Usage:  extract_edges <input_file> <output_csv>
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::fprintf(stderr,
            "Usage: extract_edges <input_file> <output_csv>\n"
            "  input_file   space-separated file: patch1 radius patch2 ...\n"
            "  output_csv   output CSV file: patch1,patch2\n");
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "Cannot open input file: %s\n", argv[1]);
        return 1;
    }

    std::ofstream out(argv[2]);
    if (!out) {
        std::fprintf(stderr, "Cannot open output file: %s\n", argv[2]);
        return 1;
    }

    std::string line;
    int lineNum   = 0;
    int written   = 0;
    while (std::getline(in, line)) {
        ++lineNum;
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream ss(line);
        std::string tok1, tok2, tok3;

        if (!(ss >> tok1 >> tok2 >> tok3)) {
            std::fprintf(stderr, "Warning: skipping malformed line %d: %s\n",
                         lineNum, line.c_str());
            continue;
        }

        // Validate that tok1 and tok3 are integers
        try {
            int p1 = std::stoi(tok1);
            std::stod(tok2);          // radius – parsed only for validation
            int p2 = std::stoi(tok3);
            out << p1 << "," << p2 << "\n";
            ++written;
        } catch (...) {
            // Skip header or non-numeric lines silently
            continue;
        }
    }

    std::printf("Done: %d edges written to %s\n", written, argv[2]);
    return 0;
}