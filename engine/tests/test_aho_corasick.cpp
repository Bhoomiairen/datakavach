#include "check.hpp"
#include "datakavach/aho_corasick.hpp"

TEST(aho_corasick_finds_overlapping_keywords) {
    dk::AhoCorasick ac;
    ac.add("he", 0);
    ac.add("she", 1);
    ac.add("his", 2);
    ac.add("hers", 3);
    ac.build();
    auto m = ac.find_all("ushers");
    // "she" (1..4), "he" (2..4), "hers" (2..6)
    CHECK_EQ(m.size(), std::size_t(3));
    CHECK_EQ(m[0].id + m[1].id, 1);  // she and he both end at 4
    CHECK_EQ(m[2].id, 3);
    CHECK_EQ(m[2].start, std::size_t(2));
}

TEST(aho_corasick_is_case_insensitive_and_utf8_safe) {
    dk::AhoCorasick ac;
    ac.add("aadhaar", 0);
    ac.add("आधार", 1);
    ac.build();
    auto m = ac.find_all("AADHAAR no. / आधार संख्या");
    CHECK_EQ(m.size(), std::size_t(2));
    CHECK_EQ(m[0].id, 0);
    CHECK_EQ(m[1].id, 1);
}
