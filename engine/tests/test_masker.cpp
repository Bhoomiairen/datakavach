#include "check.hpp"
#include "datakavach/detector.hpp"
#include "datakavach/masker.hpp"

namespace {
dk::MaskResult run(const std::string& text, dk::MaskMode mode) {
    static dk::Detector d;
    return dk::mask(text, d.detect(text), mode);
}
}  // namespace

TEST(partial_keeps_format_and_last_four) {
    CHECK_EQ(run("Aadhaar 7260 1815 9082", dk::MaskMode::Partial).text, std::string("Aadhaar XXXX XXXX 9082"));
    CHECK_EQ(dk::partial_mask(dk::PiiType::Pan, "JNEPR1948V"), std::string("XXXXXX948V"));
    CHECK_EQ(dk::partial_mask(dk::PiiType::Email, "airenbhoomi@gmail.com"), std::string("p***@gmail.com"));
}

TEST(redact_uses_type_labels) {
    CHECK_EQ(run("call 9876543210 now", dk::MaskMode::Redact).text, std::string("call [PHONE] now"));
}

TEST(tokens_are_consistent_and_reversible) {
    const std::string text = "call 9876543210, again 9876543210, or 9123456780";
    auto r = run(text, dk::MaskMode::Token);
    CHECK_EQ(r.text, std::string("call [PHONE_1], again [PHONE_1], or [PHONE_2]"));
    CHECK_EQ(r.vault.size(), std::size_t(2));
    CHECK_EQ(dk::unmask(r.text, r.vault), text);
    CHECK_EQ(dk::unmask("Reply to [PHONE_2] and [UNKNOWN_9]", r.vault),
             std::string("Reply to 9123456780 and [UNKNOWN_9]"));
}

TEST(partial_mask_on_devanagari_digits) {
    CHECK_EQ(dk::partial_mask(dk::PiiType::Aadhaar, "७२६० १८१५ ९०८२"), std::string("XXXX XXXX ९०८२"));
}
