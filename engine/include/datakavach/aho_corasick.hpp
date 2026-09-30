#pragma once
// Aho-Corasick automaton: finds every occurrence of many keywords in ONE pass over the text.
//
// Why: DataKavach looks for ~110 context keywords ("aadhaar", "mobile", "ifsc", "आधार", ...).
// Searching for each keyword separately would read the text 110 times. Aho-Corasick builds a
// trie of all keywords, adds "failure links" (where to continue after a mismatch, like KMP),
// and turns it into a full state machine (DFA). Then each input byte is ONE table lookup,
// so the cost is O(text length + number of matches), independent of how many keywords there are.
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dk {

class AhoCorasick {
public:
    // Adds a keyword (matched case-insensitively for ASCII letters). Call before build().
    void add(std::string_view keyword, int id);

    // Builds failure links and the DFA. Must be called once, after all add() calls.
    void build();

    struct Match {
        std::size_t start;  // byte offset of the first byte of the keyword
        std::size_t end;    // byte offset just after the keyword
        int id;
    };

    // Reports every keyword occurrence (overlapping ones included), in order of end position.
    std::vector<Match> find_all(std::string_view text) const;

    std::size_t state_count() const { return next_.size(); }

private:
    struct Out {
        int id;
        uint32_t length;
    };
    std::vector<std::array<int32_t, 256>> next_;  // DFA transitions
    std::vector<int32_t> fail_;
    std::vector<std::vector<Out>> out_;           // keywords ending at each state (after merge)
    bool built_ = false;

    int new_state();
};

}  // namespace dk
