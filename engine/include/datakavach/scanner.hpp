#pragma once
// The scanner: walks folders, reads files in parallel, and builds a report of where personal
// data lives - per file, per CSV column, per type - without ever storing the raw values.
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "datakavach/detector.hpp"

namespace dk {

struct ScanOptions {
    unsigned threads = 0;           // 0 = number of CPU cores
    double threshold = 0.5;
    std::size_t samples_per_file = 8;
    bool show_values = false;       // include raw values in samples (off by default!)
};

struct Sample {
    PiiType type;
    std::size_t line;               // 1-based
    int column = -1;                // CSV column index, -1 if not a CSV
    double score;
    std::string shown;              // partially masked value (or raw with show_values)
};

struct ColumnStats {
    std::string name;
    std::array<uint64_t, kTypeCount> counts{};
};

struct FileReport {
    std::string path;
    uint64_t bytes = 0;
    uint64_t lines = 0;
    bool tabular = false;           // CSV / TSV: findings are attributed to columns
    std::array<uint64_t, kTypeCount> counts{};
    std::vector<ColumnStats> columns;
    std::vector<Sample> samples;
    std::string error;              // non-empty if the file could not be read

    uint64_t total() const;
};

struct ScanReport {
    std::vector<FileReport> files;
    uint64_t bytes = 0;
    uint64_t skipped = 0;           // binary or unsupported files
    double seconds = 0;
    unsigned threads = 1;
    double threshold = 0.5;
    std::array<uint64_t, kTypeCount> totals{};
};

// Files that will be scanned under the given paths (text-like files, skips .git, node_modules...).
std::vector<std::filesystem::path> collect_files(const std::vector<std::string>& roots,
                                                 uint64_t& skipped);

// Scans one file (used by scan() and by tests).
FileReport scan_file(const Detector& detector, const std::filesystem::path& path,
                     const ScanOptions& options);

ScanReport scan(const Detector& detector, const std::vector<std::string>& roots,
                const ScanOptions& options);

std::string report_to_json(const ScanReport& report);

}  // namespace dk
