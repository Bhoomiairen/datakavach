#include "check.hpp"

int main() {
    int failed_cases = 0;
    for (const auto& c : dktest::registry()) {
        const int before = dktest::failures();
        c.fn();
        const bool ok = dktest::failures() == before;
        if (!ok) ++failed_cases;
        std::cout << (ok ? "  ok    " : "  FAILED ") << c.name << "\n";
    }
    std::cout << "\n" << dktest::registry().size() - failed_cases << "/" << dktest::registry().size()
              << " tests passed\n";
    return failed_cases == 0 ? 0 : 1;
}
