#include "datakavach/scanner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>
#include <unordered_set>

#include "datakavach/json.hpp"
#include "datakavach/masker.hpp"

namespace fs = std::filesystem;

namespace dk {

uint64_t FileReport::total() const {
    uint64_t t = 0;
    for (auto c : counts) t += c;
    return t;
}

namespace {

const std::unordered_set<std::string> kTextExtensions = {
    ".txt", ".csv", ".tsv", ".json", ".jsonl", ".ndjson", ".log", ".md", ".xml", ".html",
    ".htm", ".sql", ".yaml", ".yml", ".ini", ".conf", ".cfg", ".env", ".eml", ".tex", ".properties",
};
const std::unordered_set<std::string> kSkipDirs = {
    ".git", "node_modules", ".venv", "venv", "__pycache__", ".next", "build", "dist", ".cache",
};

std::string lower_ext(const fs::path& p) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return std::tolower(c); });
    return e;
}

// Splits a CSV/TSV header line, respecting "quoted, fields".
std::vector<std::string> split_header(std::string_view line, char sep) {
    std::vector<std::string> cells(1);
    bool quoted = false;
    for (char c : line) {
        if (c == '"') quoted = !quoted;
        else if (c == sep && !quoted) cells.emplace_back();
        else if (c != '\r') cells.back().push_back(c);
    }
    return cells;
}

// Column index of `offset`, counting separators outside quotes from the start of its line.
int column_at(std::string_view chunk, std::size_t line_start, std::size_t offset, char sep) {
    int col = 0;
    bool quoted = false;
    for (std::size_t i = line_start; i < offset; ++i) {
        if (chunk[i] == '"') quoted = !quoted;
        else if (chunk[i] == sep && !quoted) ++col;
    }
    return col;
}

bool looks_binary(const char* data, std::size_t n) {
    return std::memchr(data, '\0', std::min<std::size_t>(n, 8192)) != nullptr;
}

constexpr std::size_t kChunk = 8u << 20;  // 8 MB; chunks always end at a newline

}  // namespace

std::vector<fs::path> collect_files(const std::vector<std::string>& roots, uint64_t& skipped) {
    std::vector<fs::path> files;
    for (const auto& root : roots) {
        std::error_code ec;
        const fs::path p(root);
        if (fs::is_regular_file(p, ec)) {
            files.push_back(p);  // an explicitly named file is always scanned
            continue;
        }
        if (!fs::is_directory(p, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            const auto& entry = *it;
            if (entry.is_directory(ec)) {
                if (kSkipDirs.count(entry.path().filename().string())) it.disable_recursion_pending();
                continue;
            }
            if (!entry.is_regular_file(ec)) continue;
            if (kTextExtensions.count(lower_ext(entry.path()))) files.push_back(entry.path());
            else ++skipped;
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

FileReport scan_file(const Detector& detector, const fs::path& path, const ScanOptions& opt) {
    FileReport rep;
    rep.path = path.string();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        rep.error = "could not open file";
        return rep;
    }
    const std::string ext = lower_ext(path);
    const char sep = ext == ".tsv" ? '\t' : ',';
    rep.tabular = ext == ".csv" || ext == ".tsv";

    std::vector<uint32_t> header_masks;  // per column: PII types named by the header
    std::vector<std::size_t> per_type_samples(kTypeCount, 0);
    std::string buf, carry;
    std::vector<char> block(kChunk);
    uint64_t line_base = 0;
    bool first_block = true;
    bool ended_with_newline = true;

    auto process = [&](std::string_view chunk) {
        // Line starts inside this chunk, for line numbers and CSV columns.
        std::vector<std::size_t> starts{0};
        for (std::size_t i = 0; i < chunk.size(); ++i)
            if (chunk[i] == '\n') starts.push_back(i + 1);
        auto line_index = [&](std::size_t off) {
            return static_cast<std::size_t>(std::upper_bound(starts.begin(), starts.end(), off) - starts.begin() - 1);
        };

        if (rep.tabular && line_base == 0 && rep.columns.empty()) {
            const std::size_t end = chunk.find('\n');
            for (auto& name : split_header(chunk.substr(0, end), sep)) {
                header_masks.push_back(detector.label_types(name));
                rep.columns.push_back({name, {}});
            }
        }

        std::vector<Finding> cands = detector.candidates(chunk);
        if (rep.tabular) {
            std::vector<Finding> kept;
            kept.reserve(cands.size());
            for (auto& f : cands) {
                const std::size_t li = line_index(f.start);
                if (line_base + li == 0) continue;  // header row
                const int col = column_at(chunk, starts[li], f.start, sep);
                // The column header is context too: a 10-digit number under "customer_mobile".
                const uint32_t label = col < static_cast<int>(header_masks.size()) ? header_masks[col] : 0;
                if (!f.context && (label >> static_cast<int>(f.type) & 1u)) {
                    // The header names this type: it outweighs "order" found elsewhere in the row.
                    if (f.negative) f.score += Detector::kNegativePenalty;
                    f.score = std::min(1.0, f.score + Detector::context_boost(f.type));
                    f.context = true;
                    f.negative = false;
                } else if (!f.context && !f.negative && (label & Detector::kNegativeLabel) &&
                           Detector::numeric_type(f.type)) {
                    f.score -= Detector::kNegativePenalty;  // a number under "order_ref"
                    f.negative = true;
                }
                kept.push_back(std::move(f));
            }
            cands.swap(kept);
        }
        for (const Finding& f : Detector::resolve(std::move(cands), opt.threshold)) {
            const int t = static_cast<int>(f.type);
            ++rep.counts[t];
            const std::size_t li = line_index(f.start);
            int col = -1;
            if (rep.tabular) {
                col = column_at(chunk, starts[li], f.start, sep);
                while (col >= static_cast<int>(rep.columns.size()))
                    rep.columns.push_back({"column " + std::to_string(rep.columns.size() + 1), {}});
                ++rep.columns[col].counts[t];
            }
            if (rep.samples.size() < opt.samples_per_file &&
                per_type_samples[t] < std::max<std::size_t>(1, opt.samples_per_file / 4)) {
                ++per_type_samples[t];
                rep.samples.push_back({f.type, line_base + li + 1, col, f.score,
                                       opt.show_values ? f.value : partial_mask(f.type, f.value)});
            }
        }
        line_base += starts.size() - 1;  // newlines in this chunk
    };

    while (in) {
        in.read(block.data(), static_cast<std::streamsize>(block.size()));
        const std::size_t n = static_cast<std::size_t>(in.gcount());
        if (n == 0) break;
        if (first_block && looks_binary(block.data(), n)) {
            rep.error = "binary file skipped";
            return rep;
        }
        first_block = false;
        rep.bytes += n;
        ended_with_newline = block[n - 1] == '\n';
        buf.assign(carry);
        buf.append(block.data(), n);
        const std::size_t cut = buf.rfind('\n');
        if (cut == std::string::npos) {
            carry.swap(buf);
            continue;
        }
        carry.assign(buf, cut + 1, std::string::npos);
        buf.resize(cut + 1);
        process(buf);
    }
    if (!carry.empty()) process(carry);
    rep.lines = line_base + ((rep.bytes > 0 && !ended_with_newline) ? 1 : 0);
    return rep;
}

ScanReport scan(const Detector& detector, const std::vector<std::string>& roots, const ScanOptions& opt) {
    ScanReport report;
    report.threshold = opt.threshold;
    const auto files = collect_files(roots, report.skipped);
    report.files.resize(files.size());

    unsigned threads = opt.threads ? opt.threads : std::max(1u, std::thread::hardware_concurrency());
    threads = std::min<unsigned>(threads, std::max<std::size_t>(1, files.size()));
    report.threads = threads;

    const auto t0 = std::chrono::steady_clock::now();
    std::atomic<std::size_t> next{0};
    auto worker = [&] {
        // Work stealing in its simplest form: each thread takes the next unscanned file.
        for (std::size_t i = next++; i < files.size(); i = next++) {
            report.files[i] = scan_file(detector, files[i], opt);
        }
    };
    std::vector<std::thread> pool;
    for (unsigned i = 1; i < threads; ++i) pool.emplace_back(worker);
    worker();
    for (auto& t : pool) t.join();
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    for (const auto& f : report.files) {
        if (f.error == "binary file skipped") ++report.skipped;
        report.bytes += f.bytes;
        for (int t = 0; t < kTypeCount; ++t) report.totals[t] += f.counts[t];
    }
    return report;
}

std::string report_to_json(const ScanReport& r) {
    std::string out;
    JsonWriter w(out);
    auto counts = [&](const std::array<uint64_t, kTypeCount>& c) {
        w.begin_object();
        for (int t = 0; t < kTypeCount; ++t)
            if (c[t]) w.field(kTypeNames[t], static_cast<unsigned long long>(c[t]));
        w.end_object();
    };
    uint64_t total = 0, with_pii = 0;
    for (auto c : r.totals) total += c;
    for (const auto& f : r.files) with_pii += f.total() > 0;

    w.begin_object();
    w.key("summary").begin_object();
    w.field("files", static_cast<unsigned long long>(r.files.size()));
    w.field("files_with_pii", static_cast<unsigned long long>(with_pii));
    w.field("skipped", static_cast<unsigned long long>(r.skipped));
    w.field("bytes", static_cast<unsigned long long>(r.bytes));
    w.field("findings", static_cast<unsigned long long>(total));
    w.field("seconds", r.seconds);
    w.field("mb_per_second", r.seconds > 0 ? (r.bytes / 1e6) / r.seconds : 0.0);
    w.field("threads", static_cast<int>(r.threads));
    w.field("threshold", r.threshold);
    w.key("by_type");
    counts(r.totals);
    w.end_object();

    w.key("files").begin_array();
    for (const auto& f : r.files) {
        w.begin_object();
        w.field("path", f.path);
        w.field("bytes", static_cast<unsigned long long>(f.bytes));
        w.field("lines", static_cast<unsigned long long>(f.lines));
        w.field("findings", static_cast<unsigned long long>(f.total()));
        if (!f.error.empty()) w.field("error", f.error);
        w.key("by_type");
        counts(f.counts);
        if (f.tabular) {
            w.key("columns").begin_array();
            for (const auto& c : f.columns) {
                w.begin_object();
                w.field("name", c.name);
                w.key("by_type");
                counts(c.counts);
                w.end_object();
            }
            w.end_array();
        }
        w.key("samples").begin_array();
        for (const auto& s : f.samples) {
            w.begin_object();
            w.field("type", type_name(s.type));
            w.field("line", static_cast<unsigned long long>(s.line));
            if (s.column >= 0) w.field("column", s.column);
            w.field("score", s.score);
            w.field("value", s.shown);
            w.end_object();
        }
        w.end_array();
        w.end_object();
    }
    w.end_array();
    w.end_object();
    return out;
}

}  // namespace dk
