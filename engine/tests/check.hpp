#pragma once
// A minimal test framework (no dependencies): TEST(name) { CHECK(expr); CHECK_EQ(a, b); }
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dktest {
struct Case {
    const char* name;
    std::function<void()> fn;
};
inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}
struct Registrar {
    Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};
}  // namespace dktest

#define DK_CONCAT2(a, b) a##b
#define DK_CONCAT(a, b) DK_CONCAT2(a, b)
#define TEST(name)                                                                  \
    static void name();                                                             \
    static dktest::Registrar DK_CONCAT(reg_, name)(#name, name);                    \
    static void name()

#define CHECK(expr)                                                                 \
    do {                                                                            \
        if (!(expr)) {                                                              \
            ++dktest::failures();                                                   \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #expr "\n"; \
        }                                                                           \
    } while (0)

#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        auto va = (a);                                                              \
        auto vb = (b);                                                              \
        if (!(va == vb)) {                                                          \
            ++dktest::failures();                                                   \
            std::ostringstream os;                                                  \
            os << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #a " == " #b     \
               << "\n       got " << va << " vs " << vb << "\n";                    \
            std::cerr << os.str();                                                  \
        }                                                                           \
    } while (0)
