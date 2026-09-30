#include "datakavach/detector.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <iterator>
#include <cstdint>
#include <set>
#include <string>

#include "datakavach/validators.hpp"

namespace dk {

namespace {

constexpr int kNegative = 100;  // keyword id for "this number is probably NOT personal data"

struct Keyword {
    std::string_view text;
    int kind;  // PiiType as int, or kNegative
};

// Context keywords. ASCII ones must stand alone as words ("pan" must not match "company").
// Hindi ones are matched anywhere, because Indian scripts attach suffixes to words.
constexpr Keyword kKeywords[] = {
    // Aadhaar
    {"aadhaar", 0}, {"aadhar", 0}, {"adhaar", 0}, {"adhar", 0}, {"uid", 0}, {"uidai", 0},
    {"uid no", 0}, {"आधार", 0},
    // PAN
    {"pan", 1}, {"pan no", 1}, {"pan card", 1}, {"pan number", 1}, {"permanent account", 1},
    {"पैन", 1},
    // GSTIN
    {"gstin", 2}, {"gst", 2}, {"gst no", 2}, {"gstn", 2}, {"जीएसटी", 2},
    // Card
    {"card", 3}, {"credit card", 3}, {"debit card", 3}, {"card no", 3}, {"visa", 3},
    {"mastercard", 3}, {"rupay", 3}, {"cc", 3}, {"कार्ड", 3},
    // Phone
    {"phone", 4}, {"mobile", 4}, {"mob", 4}, {"mob no", 4}, {"mobile no", 4}, {"ph", 4},
    {"ph no", 4}, {"phn", 4}, {"fone", 4}, {"contact", 4}, {"contact no", 4}, {"cell", 4},
    {"tel", 4}, {"call", 4}, {"whatsapp", 4}, {"wa", 4}, {"msisdn", 4}, {"मोबाइल", 4},
    {"फोन", 4}, {"फ़ोन", 4},
    // Bank account
    {"account", 5}, {"account no", 5}, {"account number", 5}, {"acct", 5}, {"a/c", 5},
    {"ac no", 5}, {"acc no", 5}, {"bank", 5}, {"खाता", 5}, {"बैंक", 5},
    // IFSC
    {"ifsc", 6}, {"ifsc code", 6}, {"branch", 6}, {"bank", 6},
    // UPI
    {"upi", 7}, {"upi id", 7}, {"vpa", 7}, {"gpay", 7}, {"google pay", 7}, {"phonepe", 7},
    {"paytm", 7}, {"bhim", 7}, {"यूपीआई", 7},
    // Email
    {"email", 8}, {"e-mail", 8}, {"mail", 8}, {"mail id", 8}, {"ईमेल", 8},
    // Voter ID
    {"voter", 9}, {"voter id", 9}, {"epic", 9}, {"epic no", 9}, {"election", 9},
    {"मतदाता", 9},
    // Passport
    {"passport", 10}, {"passport no", 10}, {"पासपोर्ट", 10},
    // Negative context: numbers after these words are usually business references, not people.
    {"order", kNegative}, {"order id", kNegative}, {"invoice", kNegative}, {"inv", kNegative},
    {"txn", kNegative}, {"transaction", kNegative}, {"ref", kNegative}, {"reference", kNegative},
    {"utr", kNegative}, {"tracking", kNegative}, {"awb", kNegative}, {"sku", kNegative},
    {"serial", kNegative}, {"imei", kNegative}, {"policy", kNegative}, {"ticket", kNegative},
    {"pnr", kNegative}, {"batch", kNegative}, {"product", kNegative}, {"item", kNegative},
    {"qty", kNegative}, {"amount", kNegative}, {"rs", kNegative}, {"inr", kNegative},
    {"barcode", kNegative}, {"ean", kNegative}, {"upc", kNegative}, {"isbn", kNegative},
    {"hsn", kNegative},
};

inline bool is_digit(unsigned char c) { return c >= '0' && c <= '9'; }
inline bool is_alpha(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_alnum(unsigned char c) { return is_digit(c) || is_alpha(c); }
inline char to_upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c; }

// ------------------------------------------------------------------ 1. normalisation

// Text with every Indian-script digit replaced by its ASCII digit, plus a map from each
// normalised byte to the original byte offset (map has one extra entry = original length).
struct Normalized {
    std::string text;
    std::vector<uint32_t> map;  // empty when the text needed no changes (identity)
    std::size_t orig_offset(std::size_t i) const { return map.empty() ? i : map[i]; }
};

// Digits of the Indian scripts: U+0966 (Devanagari), U+09E6 (Bengali), U+0A66 (Gurmukhi),
// U+0AE6 (Gujarati), U+0B66 (Odia), U+0BE6 (Tamil), U+0C66 (Telugu), U+0CE6 (Kannada),
// U+0D66 (Malayalam). In every block the digits are at offset 0x66-0x6F. All are 3 bytes in UTF-8.
int indic_digit(std::string_view s, std::size_t i) {
    if (i + 2 >= s.size()) return -1;
    const auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 != 0xE0) return -1;
    const auto b1 = static_cast<unsigned char>(s[i + 1]);
    const auto b2 = static_cast<unsigned char>(s[i + 2]);
    const unsigned cp = ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
    if (cp < 0x0900 || cp > 0x0DFF) return -1;
    const unsigned low = cp & 0x7F;
    if (low < 0x66 || low > 0x6F) return -1;
    return static_cast<int>(low - 0x66);
}

Normalized normalize(std::string_view s) {
    Normalized n;
    bool any = false;
    for (std::size_t i = 0; i + 2 < s.size(); ++i) {
        if (static_cast<unsigned char>(s[i]) == 0xE0 && indic_digit(s, i) >= 0) {
            any = true;
            break;
        }
    }
    if (!any) {
        n.text.assign(s);
        return n;
    }
    n.text.reserve(s.size());
    n.map.reserve(s.size() + 1);
    for (std::size_t i = 0; i < s.size();) {
        int d = indic_digit(s, i);
        if (d >= 0) {
            n.text.push_back(static_cast<char>('0' + d));
            n.map.push_back(static_cast<uint32_t>(i));
            i += 3;
        } else {
            n.text.push_back(s[i]);
            n.map.push_back(static_cast<uint32_t>(i));
            ++i;
        }
    }
    n.map.push_back(static_cast<uint32_t>(s.size()));
    return n;
}

// ------------------------------------------------------------------ 2. tokens

struct Token {
    std::size_t start, end;
    bool digits_only;
};

std::vector<Token> tokenize(std::string_view t) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    while (i < t.size()) {
        if (!is_alnum(static_cast<unsigned char>(t[i]))) {
            ++i;
            continue;
        }
        std::size_t j = i;
        bool digits = true;
        while (j < t.size() && is_alnum(static_cast<unsigned char>(t[j]))) {
            digits = digits && is_digit(static_cast<unsigned char>(t[j]));
            ++j;
        }
        tokens.push_back({i, j, digits});
        i = j;
    }
    return tokens;
}

// A decimal like "1234567890.50" or a digit run glued to a decimal point is an amount, not an ID.
bool part_of_decimal(std::string_view t, std::size_t start, std::size_t end) {
    const bool dot_after = end + 1 < t.size() && t[end] == '.' &&
                           is_digit(static_cast<unsigned char>(t[end + 1]));
    const bool dot_before = start >= 2 && t[start - 1] == '.' &&
                            is_digit(static_cast<unsigned char>(t[start - 2]));
    return dot_after || dot_before;
}

// ------------------------------------------------------------------ 3. candidates

struct Candidate {
    PiiType type;
    std::size_t start, end;  // offsets in the NORMALISED text
    double base;
    bool numeric;            // a number-shaped ID (negative context applies)
};

void add_alnum_candidates(std::string_view t, const Token& tok, std::vector<Candidate>& out) {
    const std::size_t len = tok.end - tok.start;
    if (len != 8 && len != 10 && len != 11 && len != 15) return;
    std::string up(t.substr(tok.start, len));
    bool had_lower = false;
    for (char& c : up) {
        if (c >= 'a' && c <= 'z') had_lower = true;
        c = to_upper(c);
    }
    // Official IDs are printed in capitals; lower-case look-alikes are more often random words.
    const double case_penalty = had_lower ? 0.35 : 0.0;
    auto push = [&](PiiType type, double base) {
        out.push_back({type, tok.start, tok.end, base - case_penalty, false});
    };
    if (len == 10) {
        if (validate::pan(up)) push(PiiType::Pan, 0.8);
        else if (validate::voter_id(up)) push(PiiType::VoterId, 0.35);
    } else if (len == 15) {
        if (validate::gstin(up)) push(PiiType::Gstin, 0.95);
        else if (validate::gstin_structure(up)) push(PiiType::Gstin, 0.4);  // typo in check char
    } else if (len == 11) {
        if (validate::ifsc(up)) push(PiiType::Ifsc, 0.65);
    } else if (len == 8) {
        if (validate::passport(up)) push(PiiType::Passport, 0.3);
    }
}

// Digit groups like "9876 5432 1012" or "+91-98765-43210" arrive as several tokens.
// Every contiguous run of groups is tried, because a line such as
// "9876543210 2345 6789 0123" holds a phone number followed by an Aadhaar number.
void add_digit_candidates(std::string_view t, const std::vector<Token>& tokens, std::size_t first,
                          std::size_t last, std::vector<Candidate>& out) {
    for (std::size_t i = first; i <= last; ++i) {
        const bool plus = tokens[i].start > 0 && t[tokens[i].start - 1] == '+';
        std::string digits;
        digits.reserve(24);
        std::vector<std::size_t> sizes;
        sizes.reserve(8);
        for (std::size_t j = i; j <= last; ++j) {
            const auto& tk = tokens[j];
            digits.append(t.substr(tk.start, tk.end - tk.start));
            sizes.push_back(tk.end - tk.start);
            if (digits.size() > 19) break;
            const std::size_t s = tokens[i].start, e = tk.end;
            if (part_of_decimal(t, s, e)) continue;
            const bool grouped = j > i;
            const std::size_t n = digits.size();
            auto push = [&](PiiType type, double base) { out.push_back({type, s, e, base, true}); };

            if (n == 12 && validate::aadhaar(digits)) {
                const bool four_four_four = sizes.size() == 3 && sizes[0] == 4 && sizes[1] == 4;
                push(PiiType::Aadhaar, four_four_four ? 0.65 : 0.55);
            }
            if (n == 10 && validate::mobile10(digits)) {
                const bool five_five = sizes.size() == 2 && sizes[0] == 5;
                if (plus) out.push_back({PiiType::Phone, s - 1, e, 0.6, true});
                else push(PiiType::Phone, five_five ? 0.55 : 0.5);
            }
            const std::string_view dv(digits);
            if (n == 12 && dv.substr(0, 2) == "91" && validate::mobile10(dv.substr(2))) {
                // +91 9301947774: the "+" belongs to the number, so masking removes it too.
                out.push_back({PiiType::Phone, plus ? s - 1 : s, e, plus ? 0.85 : 0.55, true});
            }
            if (n == 11 && dv[0] == '0' && validate::mobile10(dv.substr(1))) {
                push(PiiType::Phone, 0.6);  // 098765 43210
            }
            if (n >= 13 && validate::card(digits)) {
                const bool fours = sizes.size() >= 3 && sizes[0] == 4;
                push(PiiType::Card, fours ? 0.7 : 0.6);
            }
            if (!grouped && n >= 9 && n <= 18) {
                push(PiiType::BankAccount, 0.2);  // only reported with "account"/"a/c" nearby
            }
        }
    }
}

void add_at_candidates(std::string_view t, std::vector<Candidate>& out) {
    auto local_char = [](unsigned char c) {
        return is_alnum(c) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-';
    };
    auto domain_char = [](unsigned char c) { return is_alnum(c) || c == '.' || c == '-'; };
    for (std::size_t at = t.find('@'); at != std::string_view::npos; at = t.find('@', at + 1)) {
        std::size_t s = at;
        while (s > 0 && local_char(static_cast<unsigned char>(t[s - 1]))) --s;
        std::size_t e = at + 1;
        while (e < t.size() && domain_char(static_cast<unsigned char>(t[e]))) ++e;
        while (e > at + 1 && (t[e - 1] == '.' || t[e - 1] == '-')) --e;
        while (s < at && (t[s] == '.' || t[s] == '-')) ++s;
        if (s == at || e == at + 1) continue;
        const std::string_view whole = t.substr(s, e - s);
        const std::string_view domain = t.substr(at + 1, e - at - 1);
        if (validate::email(whole)) {
            out.push_back({PiiType::Email, s, e, 0.95, false});
        } else if (domain.find('.') == std::string_view::npos) {
            const bool known = validate::known_upi_handle(domain);
            out.push_back({PiiType::Upi, s, e, known ? 0.9 : 0.3, false});
        }
    }
}

// ------------------------------------------------------------------ 4. context

// Positions of keyword matches, per kind, so "is there a keyword just before X" is a binary search.
struct ContextIndex {
    std::array<std::vector<std::size_t>, kTypeCount> ends;    // keyword end offsets (sorted)
    std::array<std::vector<std::size_t>, kTypeCount> starts;  // keyword start offsets (sorted)
    std::vector<std::size_t> negative_ends;

    // Distance from the closest element of v in [lo, hi] to `anchor`, or SIZE_MAX if none.
    static std::size_t closest_before(const std::vector<std::size_t>& v, std::size_t lo, std::size_t anchor) {
        auto it = std::upper_bound(v.begin(), v.end(), anchor);  // last element <= anchor
        if (it == v.begin()) return SIZE_MAX;
        const std::size_t pos = *std::prev(it);
        return pos >= lo ? anchor - pos : SIZE_MAX;
    }
    static std::size_t closest_after(const std::vector<std::size_t>& v, std::size_t anchor, std::size_t hi) {
        auto it = std::lower_bound(v.begin(), v.end(), anchor);
        return (it != v.end() && *it <= hi) ? *it - anchor : SIZE_MAX;
    }
};

// How far before a candidate a keyword may be. Bank-account numbers have no shape of their own,
// so their keyword must be right next to them ("A/c no 5010..."), not a sentence away.
std::size_t window_before(PiiType t) { return t == PiiType::BankAccount ? 20 : 48; }
constexpr std::size_t kAfter = 24;   // "9876543210 (mobile)"
constexpr std::size_t kNegBefore = 32;

}  // namespace

// ------------------------------------------------------------------ Detector

bool type_from_name(std::string_view name, PiiType& out) {
    for (int i = 0; i < kTypeCount; ++i) {
        if (kTypeNames[i] == name) {
            out = static_cast<PiiType>(i);
            return true;
        }
    }
    return false;
}

Detector::Detector() {
    for (std::size_t i = 0; i < std::size(kKeywords); ++i) keywords_.add(kKeywords[i].text, static_cast<int>(i));
    keywords_.build();
}

bool Detector::numeric_type(PiiType t) {
    return t == PiiType::Aadhaar || t == PiiType::Phone || t == PiiType::Card || t == PiiType::BankAccount;
}

double Detector::context_boost(PiiType t) {
    switch (t) {
        case PiiType::BankAccount: return 0.4;
        case PiiType::Upi: return 0.4;
        case PiiType::VoterId: return 0.35;
        case PiiType::Passport: return 0.35;
        default: return 0.3;
    }
}

namespace {
// ASCII keywords must be whole words: the bytes around them must not be letters.
bool standalone(std::string_view t, std::size_t s, std::size_t e, std::string_view kw) {
    if (static_cast<unsigned char>(kw.front()) >= 0x80) return true;  // Indian-script keyword
    const bool left_ok = s == 0 || !is_alpha(static_cast<unsigned char>(t[s - 1]));
    const bool right_ok = e >= t.size() || !is_alpha(static_cast<unsigned char>(t[e]));
    return left_ok && right_ok;
}
}  // namespace

uint32_t Detector::label_types(std::string_view label) const {
    uint32_t mask = 0;
    for (const auto& m : keywords_.find_all(label)) {
        const Keyword& kw = kKeywords[m.id];
        if (!standalone(label, m.start, m.end, kw.text)) continue;
        mask |= kw.kind == kNegative ? kNegativeLabel : (1u << kw.kind);
    }
    return mask;
}

std::vector<Finding> Detector::candidates(std::string_view original) const {
    const Normalized norm = normalize(original);
    const std::string_view t = norm.text;

    // 2-3. tokens and candidates
    std::vector<Candidate> cands;
    const std::vector<Token> tokens = tokenize(t);
    for (std::size_t i = 0; i < tokens.size();) {
        if (!tokens[i].digits_only) {
            add_alnum_candidates(t, tokens[i], cands);
            ++i;
            continue;
        }
        // Extend over digit tokens separated by exactly one space or hyphen.
        std::size_t j = i;
        while (j + 1 < tokens.size() && tokens[j + 1].digits_only &&
               tokens[j + 1].start == tokens[j].end + 1 &&
               (t[tokens[j].end] == ' ' || t[tokens[j].end] == '-')) {
            ++j;
        }
        add_digit_candidates(t, tokens, i, j, cands);
        i = j + 1;
    }
    add_at_candidates(t, cands);
    if (cands.empty()) return {};

    // 4. context
    ContextIndex ctx;
    for (const auto& m : keywords_.find_all(t)) {
        const Keyword& kw = kKeywords[m.id];
        if (!standalone(t, m.start, m.end, kw.text)) continue;
        if (kw.kind == kNegative) {
            ctx.negative_ends.push_back(m.end);
        } else {
            ctx.ends[kw.kind].push_back(m.end);
            ctx.starts[kw.kind].push_back(m.start);
        }
    }
    for (auto& v : ctx.starts) std::sort(v.begin(), v.end());

    std::vector<Finding> found;
    found.reserve(cands.size());
    for (const Candidate& c : cands) {
        const int k = static_cast<int>(c.type);
        const std::size_t win = window_before(c.type);
        const std::size_t lo = c.start > win ? c.start - win : 0;
        const std::size_t pos_dist =
            std::min(ContextIndex::closest_before(ctx.ends[k], lo, c.start),
                     ContextIndex::closest_after(ctx.starts[k], c.end, c.end + kAfter));
        std::size_t neg_dist = SIZE_MAX;
        if (c.numeric) {
            const std::size_t nlo = c.start > kNegBefore ? c.start - kNegBefore : 0;
            neg_dist = ContextIndex::closest_before(ctx.negative_ends, nlo, c.start);
        }
        // The nearest keyword wins: in "wrong account. Order ID 2679..." the number belongs to
        // "Order ID", not to "account".
        const bool positive = pos_dist != SIZE_MAX && pos_dist <= neg_dist;
        const bool negative = neg_dist != SIZE_MAX && neg_dist < pos_dist;
        double score = c.base;
        if (positive) score += context_boost(c.type);
        if (negative) score -= kNegativePenalty;
        Finding f;
        f.type = c.type;
        f.start = norm.orig_offset(c.start);
        f.end = norm.orig_offset(c.end);
        f.score = std::clamp(score, 0.0, 1.0);
        f.context = positive;
        f.negative = negative;
        f.value.assign(original.substr(f.start, f.end - f.start));
        found.push_back(std::move(f));
    }
    return found;
}

std::vector<Finding> Detector::resolve(std::vector<Finding> cands, double threshold) {
    cands.erase(std::remove_if(cands.begin(), cands.end(),
                               [&](const Finding& f) { return f.score + 1e-9 < threshold; }),
                cands.end());
    // Best first: higher score, then longer span (a GSTIN beats the PAN inside it).
    std::sort(cands.begin(), cands.end(), [](const Finding& a, const Finding& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.end - a.start != b.end - b.start) return a.end - a.start > b.end - b.start;
        return a.start < b.start;
    });
    // Greedy interval selection: an ordered set of accepted [start, end) spans.
    std::set<std::pair<std::size_t, std::size_t>> taken;
    std::vector<Finding> kept;
    for (auto& f : cands) {
        auto it = taken.lower_bound({f.start, 0});
        const bool overlaps_next = it != taken.end() && it->first < f.end;
        const bool overlaps_prev = it != taken.begin() && std::prev(it)->second > f.start;
        if (overlaps_next || overlaps_prev) continue;
        taken.insert({f.start, f.end});
        kept.push_back(std::move(f));
    }
    std::sort(kept.begin(), kept.end(),
              [](const Finding& a, const Finding& b) { return a.start < b.start; });
    return kept;
}

std::vector<Finding> Detector::detect(std::string_view text, double threshold) const {
    return resolve(candidates(text), threshold);
}

}  // namespace dk
