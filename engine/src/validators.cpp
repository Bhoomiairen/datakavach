#include "datakavach/validators.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace dk::validate {

namespace {

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
inline bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
inline bool is_upper_or_digit(char c) { return is_upper(c) || is_digit(c); }

bool all_digits(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), is_digit);
}

bool all_same(std::string_view s) {
    return std::all_of(s.begin(), s.end(), [&](char c) { return c == s.front(); });
}

// Verhoeff tables: multiplication in the dihedral group D5, a position permutation, inverses.
constexpr int kD[10][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 4, 0, 6, 7, 8, 9, 5},
    {2, 3, 4, 0, 1, 7, 8, 9, 5, 6}, {3, 4, 0, 1, 2, 8, 9, 5, 6, 7},
    {4, 0, 1, 2, 3, 9, 5, 6, 7, 8}, {5, 9, 8, 7, 6, 0, 4, 3, 2, 1},
    {6, 5, 9, 8, 7, 1, 0, 4, 3, 2}, {7, 6, 5, 9, 8, 2, 1, 0, 4, 3},
    {8, 7, 6, 5, 9, 3, 2, 1, 0, 4}, {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
};
constexpr int kP[8][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 5, 7, 6, 2, 8, 3, 0, 9, 4},
    {5, 8, 0, 3, 7, 9, 6, 1, 4, 2}, {8, 9, 1, 6, 0, 4, 3, 5, 2, 7},
    {9, 4, 5, 3, 1, 2, 6, 8, 7, 0}, {4, 2, 8, 6, 5, 7, 3, 9, 0, 1},
    {2, 7, 9, 3, 8, 0, 6, 4, 1, 5}, {7, 0, 4, 6, 9, 1, 3, 2, 5, 8},
};

constexpr std::string_view kBase36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// GST state / UT codes: 01-38, plus 97 (other territory) and 99 (centre jurisdiction).
bool valid_state_code(int code) { return (code >= 1 && code <= 38) || code == 97 || code == 99; }

}  // namespace

bool verhoeff(std::string_view digits) {
    if (!all_digits(digits)) return false;
    int c = 0;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        int d = digits[n - 1 - i] - '0';
        c = kD[c][kP[i % 8][d]];
    }
    return c == 0;
}

bool luhn(std::string_view digits) {
    if (!all_digits(digits)) return false;
    int sum = 0;
    bool dbl = false;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        int d = *it - '0';
        if (dbl) {
            d *= 2;
            if (d > 9) d -= 9;
        }
        sum += d;
        dbl = !dbl;
    }
    return sum % 10 == 0;
}

bool aadhaar(std::string_view d) {
    return d.size() == 12 && all_digits(d) && d[0] >= '2' && !all_same(d) && verhoeff(d);
}

bool pan(std::string_view s) {
    if (s.size() != 10) return false;
    for (int i = 0; i < 5; ++i)
        if (!is_upper(s[i])) return false;
    for (int i = 5; i < 9; ++i)
        if (!is_digit(s[i])) return false;
    if (!is_upper(s[9])) return false;
    // 4th character: holder type. P=person, C=company, H=HUF, F=firm, A=association of persons,
    // T=trust, B=body of individuals, L=local authority, J=artificial juridical person, G=government,
    // K=krish (used by some state PANs).
    static constexpr std::string_view kHolder = "PCHFATBLJGK";
    return kHolder.find(s[3]) != std::string_view::npos;
}

char gstin_check_char(std::string_view first14) {
    if (first14.size() != 14) return '\0';
    int sum = 0;
    for (int i = 0; i < 14; ++i) {
        std::size_t v = kBase36.find(first14[i]);
        if (v == std::string_view::npos) return '\0';
        int prod = static_cast<int>(v) * ((i % 2) ? 2 : 1);
        sum += prod / 36 + prod % 36;
    }
    return kBase36[(36 - sum % 36) % 36];
}

bool gstin_structure(std::string_view s) {
    if (s.size() != 15) return false;
    if (!is_digit(s[0]) || !is_digit(s[1])) return false;
    if (!valid_state_code((s[0] - '0') * 10 + (s[1] - '0'))) return false;
    if (!pan(s.substr(2, 10))) return false;
    if (!is_upper_or_digit(s[12]) || s[12] == '0') return false;  // entity number 1-9 or A-Z
    if (s[13] != 'Z') return false;
    return is_upper_or_digit(s[14]);
}

bool gstin(std::string_view s) {
    return gstin_structure(s) && gstin_check_char(s.substr(0, 14)) == s[14];
}

bool card(std::string_view d) {
    if (d.size() < 13 || d.size() > 19 || !all_digits(d) || all_same(d)) return false;
    auto pre = [&](std::size_t n) {
        int v = 0;
        for (std::size_t i = 0; i < n; ++i) v = v * 10 + (d[i] - '0');
        return v;
    };
    const int p1 = d[0] - '0';
    const int p2 = pre(2), p3 = pre(3), p4 = pre(4);
    bool network = false;
    if (p1 == 4 && (d.size() == 13 || d.size() == 16 || d.size() == 19)) network = true;   // Visa
    if (((p2 >= 51 && p2 <= 55) || (p4 >= 2221 && p4 <= 2720)) && d.size() == 16) network = true;  // Mastercard
    if ((p2 == 34 || p2 == 37) && d.size() == 15) network = true;                           // Amex
    if ((p2 == 60 || p2 == 65 || p2 == 81 || p2 == 82 || p3 == 508 || p3 == 353 || p3 == 356) &&
        d.size() == 16)
        network = true;                                                                      // RuPay
    if ((p4 == 6011 || p2 == 64) && d.size() == 16) network = true;                          // Discover
    if (p2 == 36 && d.size() == 14) network = true;                                          // Diners
    return network && luhn(d);
}

bool mobile10(std::string_view d) {
    return d.size() == 10 && all_digits(d) && d[0] >= '6' && !all_same(d);
}

bool ifsc(std::string_view s) {
    if (s.size() != 11) return false;
    for (int i = 0; i < 4; ++i)
        if (!is_upper(s[i])) return false;
    if (s[4] != '0') return false;
    for (int i = 5; i < 11; ++i)
        if (!is_upper_or_digit(s[i])) return false;
    return true;
}

bool voter_id(std::string_view s) {
    if (s.size() != 10) return false;
    for (int i = 0; i < 3; ++i)
        if (!is_upper(s[i])) return false;
    for (int i = 3; i < 10; ++i)
        if (!is_digit(s[i])) return false;
    return true;
}

bool passport(std::string_view s) {
    if (s.size() != 8 || !is_upper(s[0])) return false;
    if (s[0] == 'Q' || s[0] == 'X' || s[0] == 'Z') return false;
    for (int i = 1; i < 8; ++i)
        if (!is_digit(s[i])) return false;
    return s[1] != '0' && s[7] != '0';
}

bool email(std::string_view s) {
    const auto at = s.find('@');
    if (at == std::string_view::npos || at == 0 || s.find('@', at + 1) != std::string_view::npos)
        return false;
    const auto domain = s.substr(at + 1);
    const auto dot = domain.rfind('.');
    if (dot == std::string_view::npos || dot == 0) return false;
    const auto tld = domain.substr(dot + 1);
    if (tld.size() < 2 || tld.size() > 12) return false;
    return std::all_of(tld.begin(), tld.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });
}

bool known_upi_handle(std::string_view h) {
    // Handles of the large UPI apps and banks (lower case). Not exhaustive: an unknown handle is
    // still reported as UPI when the text around it mentions UPI / VPA.
    static constexpr std::array<std::string_view, 62> kHandles = {
        "abfspay", "airtel", "apl", "aubank", "axisb", "axisbank", "axl", "barodampay",
        "boi", "centralbank", "citi", "cnrb", "dbs", "equitas", "fbl", "federal",
        "freecharge", "hdfcbank", "hsbc", "ibl", "icici", "idbi", "idfcbank", "ikwik",
        "indus", "jio", "jupiteraxis", "kbl", "kotak", "kmbl", "mahb", "mbk",
        "naviaxis", "okaxis", "okbizaxis", "okhdfcbank", "okicici", "oksbi", "paytm", "pingpay",
        "pnb", "postbank", "ptaxis", "pthdfc", "ptsbi", "ptyes", "rbl", "sbi",
        "sc", "sib", "slc", "superyes", "timecosmos", "uco", "unionbank", "upi",
        "waaxis", "wahdfcbank", "waicici", "wasbi", "ybl", "yapl",
    };
    std::string lower(h);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(kHandles.begin(), kHandles.end(), std::string_view(lower)) != kHandles.end();
}

}  // namespace dk::validate
