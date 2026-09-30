#include "datakavach/masker.hpp"

#include <array>

namespace dk {

namespace {
inline bool is_alnum(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
}  // namespace

bool mask_mode_from_name(std::string_view name, MaskMode& out) {
    if (name == "redact") out = MaskMode::Redact;
    else if (name == "partial") out = MaskMode::Partial;
    else if (name == "token") out = MaskMode::Token;
    else return false;
    return true;
}

std::string partial_mask(PiiType type, std::string_view value) {
    if (type == PiiType::Email || type == PiiType::Upi) {
        // airenbhoomi@gmail.com -> p***@gmail.com
        const auto at = value.find('@');
        if (at == std::string_view::npos) return std::string(value.size(), '*');
        std::string out;
        out.push_back(value.front());
        out += "***";
        out.append(value.substr(at));
        return out;
    }
    // Everything else: hide every letter/digit except the last 4, keep spaces and hyphens.
    // Non-ASCII digits (e.g. Devanagari) count as one character each.
    std::size_t total = 0;
    for (std::size_t i = 0; i < value.size();) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c == 0xE0) { ++total; i += 3; }
        else { if (is_alnum(c)) ++total; ++i; }
    }
    std::string out;
    std::size_t seen = 0;
    for (std::size_t i = 0; i < value.size();) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c == 0xE0 && i + 2 < value.size()) {
            ++seen;
            if (seen + 4 > total) out.append(value.substr(i, 3));
            else out.push_back('X');
            i += 3;
        } else {
            if (is_alnum(c)) {
                ++seen;
                out.push_back(seen + 4 > total ? static_cast<char>(c) : 'X');
            } else {
                out.push_back(static_cast<char>(c));
            }
            ++i;
        }
    }
    return out;
}

MaskResult mask(std::string_view text, const std::vector<Finding>& findings, MaskMode mode) {
    MaskResult r;
    r.text.reserve(text.size());
    std::array<int, kTypeCount> counters{};
    std::map<std::pair<int, std::string>, std::string> token_of;  // (type, value) -> token
    std::size_t pos = 0;
    for (const Finding& f : findings) {
        if (f.start < pos || f.end > text.size()) continue;  // defensive: overlapping input
        r.text.append(text.substr(pos, f.start - pos));
        const std::string_view value = text.substr(f.start, f.end - f.start);
        switch (mode) {
            case MaskMode::Redact:
                r.text += "[";
                r.text += type_name(f.type);
                r.text += "]";
                break;
            case MaskMode::Partial:
                r.text += partial_mask(f.type, value);
                break;
            case MaskMode::Token: {
                const auto key = std::make_pair(static_cast<int>(f.type), std::string(value));
                auto it = token_of.find(key);
                if (it == token_of.end()) {
                    const int n = ++counters[static_cast<int>(f.type)];
                    std::string token = "[" + std::string(type_name(f.type)) + "_" + std::to_string(n) + "]";
                    it = token_of.emplace(key, token).first;
                    r.vault[token] = std::string(value);
                }
                r.text += it->second;
                break;
            }
        }
        pos = f.end;
    }
    r.text.append(text.substr(pos));
    return r;
}

std::string unmask(std::string_view text, const std::map<std::string, std::string>& vault) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '[') {
            const auto close = text.find(']', i);
            if (close != std::string_view::npos && close - i < 40) {
                auto it = vault.find(std::string(text.substr(i, close - i + 1)));
                if (it != vault.end()) {
                    out += it->second;
                    i = close + 1;
                    continue;
                }
            }
        }
        out.push_back(text[i++]);
    }
    return out;
}

}  // namespace dk
