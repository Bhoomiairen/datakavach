// datakavach: command-line interface.
//
//   datakavach scan  <folder or file>...   where is the personal data? (JSON report)
//   datakavach detect [file|-]             every finding with offsets and scores (JSON)
//   datakavach mask  [file|-] --mode ...   the text with personal data hidden
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "datakavach/detector.hpp"
#include "datakavach/json.hpp"
#include "datakavach/masker.hpp"
#include "datakavach/scanner.hpp"

namespace {

constexpr const char* kVersion = "1.0.0";

void usage() {
    std::cerr <<
R"(DataKavach - finds and masks Indian personal data (Aadhaar, PAN, GSTIN, UPI, phone, ...)

Usage:
  datakavach scan <path>... [--threads N] [--threshold 0.5] [--out report.json]
                            [--samples 8] [--show-values]
  datakavach detect [file|-] [--threshold 0.5] [--nul]
  datakavach mask   [file|-] [--mode partial|redact|token] [--threshold 0.5] [--json]
  datakavach version

  scan     walks folders in parallel and reports where personal data lives, per file and
           per CSV column. Values in the report are partially masked unless --show-values.
  detect   prints every finding as JSON (byte offsets, type, score). With --nul the input is
           several documents separated by a NUL byte; one JSON line is printed per document.
  mask     prints the text with personal data hidden. --json also returns the findings and,
           in token mode, the vault that maps tokens back to the original values.
)";
}

std::string read_input(const std::string& path) {
    if (path.empty() || path == "-") {
        return std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_findings(dk::JsonWriter& w, const std::vector<dk::Finding>& findings) {
    w.begin_array();
    for (const auto& f : findings) {
        w.begin_object();
        w.field("type", dk::type_name(f.type));
        w.field("start", static_cast<unsigned long long>(f.start));
        w.field("end", static_cast<unsigned long long>(f.end));
        w.field("score", f.score);
        w.field("context", f.context);
        w.field("negative", f.negative);
        w.field("value", f.value);
        w.end_object();
    }
    w.end_array();
}

struct Args {
    std::vector<std::string> positional;
    unsigned threads = 0;
    double threshold = 0.5;
    std::string out, mode = "partial";
    std::size_t samples = 8;
    bool show_values = false, nul = false, json = false;
};

Args parse(int argc, char** argv, int from) {
    Args a;
    for (int i = from; i < argc; ++i) {
        std::string s = argv[i];
        auto need = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(s + " needs a value");
            return argv[++i];
        };
        if (s == "--threads") a.threads = static_cast<unsigned>(std::stoul(need()));
        else if (s == "--threshold") a.threshold = std::stod(need());
        else if (s == "--out") a.out = need();
        else if (s == "--mode") a.mode = need();
        else if (s == "--samples") a.samples = std::stoul(need());
        else if (s == "--show-values") a.show_values = true;
        else if (s == "--nul") a.nul = true;
        else if (s == "--json") a.json = true;
        else if (s.size() > 1 && s[0] == '-' && s != "-") throw std::runtime_error("unknown option " + s);
        else a.positional.push_back(s);
    }
    return a;
}

void print_summary(const dk::ScanReport& r) {
    uint64_t total = 0, with = 0;
    for (auto c : r.totals) total += c;
    for (const auto& f : r.files) with += f.total() > 0;
    std::fprintf(stderr, "\nScanned %zu files (%.1f MB) in %.2fs on %u threads: %.0f MB/s\n",
                 r.files.size(), r.bytes / 1e6, r.seconds, r.threads,
                 r.seconds > 0 ? r.bytes / 1e6 / r.seconds : 0.0);
    std::fprintf(stderr, "%llu findings in %llu files\n\n", static_cast<unsigned long long>(total),
                 static_cast<unsigned long long>(with));
    for (int t = 0; t < dk::kTypeCount; ++t) {
        if (r.totals[t])
            std::fprintf(stderr, "  %-13s %8llu\n", std::string(dk::kTypeNames[t]).c_str(),
                         static_cast<unsigned long long>(r.totals[t]));
    }
    std::fprintf(stderr, "\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string cmd = argv[1];
    try {
        if (cmd == "version" || cmd == "--version") {
            std::cout << "datakavach " << kVersion << "\n";
            return 0;
        }
        if (cmd == "help" || cmd == "--help" || cmd == "-h") {
            usage();
            return 0;
        }
        const Args a = parse(argc, argv, 2);
        const dk::Detector detector;

        if (cmd == "scan") {
            if (a.positional.empty()) throw std::runtime_error("scan needs at least one path");
            dk::ScanOptions opt;
            opt.threads = a.threads;
            opt.threshold = a.threshold;
            opt.samples_per_file = a.samples;
            opt.show_values = a.show_values;
            const auto report = dk::scan(detector, a.positional, opt);
            const std::string json = dk::report_to_json(report);
            if (a.out.empty()) {
                std::cout << json << "\n";
            } else {
                std::ofstream(a.out, std::ios::binary) << json << "\n";
                std::fprintf(stderr, "Report written to %s\n", a.out.c_str());
            }
            print_summary(report);
            return 0;
        }

        const std::string input = read_input(a.positional.empty() ? "-" : a.positional[0]);

        if (cmd == "detect") {
            std::string out;
            if (a.nul) {
                std::size_t start = 0;
                while (true) {
                    std::size_t end = input.find('\0', start);
                    if (end == std::string::npos) end = input.size();
                    const auto doc = std::string_view(input).substr(start, end - start);
                    std::string line;
                    dk::JsonWriter w(line);
                    write_findings(w, detector.detect(doc, a.threshold));
                    out += line;
                    out += '\n';
                    if (end >= input.size() || end + 1 == input.size()) break;  // trailing NUL
                    start = end + 1;
                }
            } else {
                dk::JsonWriter w(out);
                w.begin_object();
                w.field("bytes", static_cast<unsigned long long>(input.size()));
                w.key("findings");
                write_findings(w, detector.detect(input, a.threshold));
                w.end_object();
                out += '\n';
            }
            std::cout << out;
            return 0;
        }

        if (cmd == "mask") {
            dk::MaskMode mode;
            if (!dk::mask_mode_from_name(a.mode, mode))
                throw std::runtime_error("--mode must be partial, redact or token");
            const auto findings = detector.detect(input, a.threshold);
            const auto result = dk::mask(input, findings, mode);
            if (!a.json) {
                std::cout << result.text;
                return 0;
            }
            std::string out;
            dk::JsonWriter w(out);
            w.begin_object();
            w.field("text", result.text);
            w.key("findings");
            write_findings(w, findings);
            w.key("vault").begin_object();
            for (const auto& [token, value] : result.vault) w.field(token, value);
            w.end_object();
            w.end_object();
            std::cout << out << "\n";
            return 0;
        }

        usage();
        return 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "datakavach: %s\n", e.what());
        return 1;
    }
}
