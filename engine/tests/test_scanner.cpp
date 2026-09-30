#include "check.hpp"
#include "datakavach/scanner.hpp"

#include <filesystem>

TEST(csv_findings_are_attributed_to_columns) {
    dk::Detector d;
    dk::ScanOptions opt;
    auto rep = dk::scan_file(d, std::filesystem::path(DK_TEST_DATA) / "customers.csv", opt);
    CHECK(rep.error.empty());
    CHECK(rep.tabular);
    CHECK_EQ(rep.lines, std::uint64_t(4));
    CHECK_EQ(rep.columns.size(), std::size_t(4));
    CHECK_EQ(rep.columns[1].name, std::string("customer_mobile"));
    // Bare 10-digit numbers: flagged thanks to the "customer_mobile" header.
    CHECK_EQ(rep.columns[1].counts[static_cast<int>(dk::PiiType::Phone)], std::uint64_t(3));
    // "order_ref" holds checksum-valid 12-digit numbers with no Aadhaar context: not reported.
    CHECK_EQ(rep.columns[3].counts[static_cast<int>(dk::PiiType::Aadhaar)], std::uint64_t(0));
    CHECK_EQ(rep.columns[2].counts[static_cast<int>(dk::PiiType::Pan)], std::uint64_t(3));
    // Samples never contain the raw value by default.
    for (const auto& s : rep.samples) CHECK(s.shown.find('X') != std::string::npos || s.shown.find('*') != std::string::npos);
}

TEST(scan_walks_folders_and_skips_binaries) {
    dk::Detector d;
    dk::ScanOptions opt;
    opt.threads = 2;
    auto rep = dk::scan(d, {DK_TEST_DATA}, opt);
    CHECK_EQ(rep.files.size(), std::size_t(2));  // customers.csv + notes.txt (image.bin skipped)
    CHECK(rep.skipped >= 1);
    CHECK(rep.totals[static_cast<int>(dk::PiiType::Aadhaar)] >= 1);
}
