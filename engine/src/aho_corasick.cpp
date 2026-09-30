#include "datakavach/aho_corasick.hpp"

#include <queue>
#include <stdexcept>

namespace dk {

namespace {
inline unsigned char fold(unsigned char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
}  // namespace

int AhoCorasick::new_state() {
    std::array<int32_t, 256> row;
    row.fill(-1);
    next_.push_back(row);
    fail_.push_back(0);
    out_.emplace_back();
    return static_cast<int>(next_.size()) - 1;
}

void AhoCorasick::add(std::string_view keyword, int id) {
    if (built_) throw std::logic_error("AhoCorasick::add after build");
    if (keyword.empty()) return;
    if (next_.empty()) new_state();  // root
    int s = 0;
    for (unsigned char raw : keyword) {
        unsigned char c = fold(raw);
        if (next_[s][c] == -1) {
            int t = new_state();  // may reallocate next_, so index again below
            next_[s][c] = t;
        }
        s = next_[s][c];
    }
    out_[s].push_back({id, static_cast<uint32_t>(keyword.size())});
}

void AhoCorasick::build() {
    if (next_.empty()) new_state();
    std::queue<int> q;
    // Depth 1: failure link to the root. Missing root transitions loop back to the root.
    for (int c = 0; c < 256; ++c) {
        int t = next_[0][c];
        if (t == -1) {
            next_[0][c] = 0;
        } else {
            fail_[t] = 0;
            q.push(t);
        }
    }
    // Breadth-first: a state's failure link is the longest proper suffix that is also in the trie.
    while (!q.empty()) {
        int s = q.front();
        q.pop();
        // Inherit the matches of the failure state ("she" also contains "he").
        const auto& inherited = out_[fail_[s]];
        out_[s].insert(out_[s].end(), inherited.begin(), inherited.end());
        for (int c = 0; c < 256; ++c) {
            int t = next_[s][c];
            if (t == -1) {
                next_[s][c] = next_[fail_[s]][c];  // DFA: jump straight to where KMP would end up
            } else {
                fail_[t] = next_[fail_[s]][c];
                q.push(t);
            }
        }
    }
    built_ = true;
}

std::vector<AhoCorasick::Match> AhoCorasick::find_all(std::string_view text) const {
    std::vector<Match> matches;
    if (!built_) throw std::logic_error("AhoCorasick::find_all before build");
    int s = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        s = next_[s][fold(static_cast<unsigned char>(text[i]))];
        for (const Out& o : out_[s]) {
            matches.push_back({i + 1 - o.length, i + 1, o.id});
        }
    }
    return matches;
}

}  // namespace dk
