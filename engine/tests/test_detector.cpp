#include "check.hpp"
#include "datakavach/detector.hpp"

#include <string>

using dk::PiiType;

namespace {
const dk::Detector& det() {
    static dk::Detector d;
    return d;
}
std::vector<dk::Finding> find(const std::string& text, double threshold = 0.5) {
    return det().detect(text, threshold);
}
bool has(const std::vector<dk::Finding>& f, PiiType t, const std::string& value) {
    for (const auto& x : f)
        if (x.type == t && x.value == value) return true;
    return false;
}
}  // namespace

TEST(finds_aadhaar_in_common_formats) {
    CHECK(has(find("Aadhaar: 7260 1815 9082"), PiiType::Aadhaar, "7260 1815 9082"));
    CHECK(has(find("uid 7260-1815-9082 verified"), PiiType::Aadhaar, "7260-1815-9082"));
    CHECK(has(find("id 726018159082"), PiiType::Aadhaar, "726018159082"));
}

TEST(ignores_12_digit_numbers_that_fail_the_checksum) {
    CHECK(find("Aadhaar: 3390 9960 3082").empty());
}

TEST(negative_context_suppresses_order_numbers) {
    // A checksum-valid 12-digit number after "order id" is treated as an order number.
    CHECK(find("Order ID 726018159082 shipped").empty());
    // ...but a keyword for the right type wins.
    CHECK(has(find("order for aadhaar 726018159082"), PiiType::Aadhaar, "726018159082"));
}

TEST(phone_numbers) {
    CHECK(has(find("call me on 9876543210"), PiiType::Phone, "9876543210"));
    CHECK(has(find("+91 9301947774"), PiiType::Phone, "+91 9301947774"));
    CHECK(has(find("mob: +91-9876543210"), PiiType::Phone, "+91-9876543210"));
    CHECK(has(find("098765 43210"), PiiType::Phone, "098765 43210"));
    CHECK(find("amount 9876543210.50 credited").empty());  // a decimal amount
}

TEST(splits_digit_groups_into_separate_ids) {
    auto f = find("9876543210 7260 1815 9082");
    CHECK(has(f, PiiType::Phone, "9876543210"));
    CHECK(has(f, PiiType::Aadhaar, "7260 1815 9082"));
}

TEST(pan_gstin_and_overlap) {
    auto f = find("GSTIN 27AAPFU0939F1ZV, PAN JNEPR1948V");
    CHECK(has(f, PiiType::Gstin, "27AAPFU0939F1ZV"));
    CHECK(has(f, PiiType::Pan, "JNEPR1948V"));
    CHECK_EQ(f.size(), std::size_t(2));  // the PAN inside the GSTIN is not reported twice
}

TEST(card_ifsc_account) {
    CHECK(has(find("card 4111 1111 1111 1111 exp 09/28"), PiiType::Card, "4111 1111 1111 1111"));
    CHECK(has(find("IFSC: HDFC0049746"), PiiType::Ifsc, "HDFC0049746"));
    CHECK(has(find("A/c no 50100234567891"), PiiType::BankAccount, "50100234567891"));
    CHECK(find("reading 50100234567891").empty());  // a long number without context
}

TEST(email_and_upi) {
    auto f = find("mail airenbhoomi@gmail.com or pay priya@okaxis / 9876543210@ybl");
    CHECK(has(f, PiiType::Email, "airenbhoomi@gmail.com"));
    CHECK(has(f, PiiType::Upi, "priya@okaxis"));
    CHECK(has(f, PiiType::Upi, "9876543210@ybl"));  // not also reported as a phone
    CHECK_EQ(f.size(), std::size_t(3));
    CHECK(has(find("UPI id: rahul@newbank"), PiiType::Upi, "rahul@newbank"));
    CHECK(find("ping rahul@newbank").empty());
}

TEST(voter_and_passport_need_context) {
    CHECK(find("code VLA7529170").empty());
    CHECK(has(find("Voter ID: VLA7529170"), PiiType::VoterId, "VLA7529170"));
    CHECK(find("model G5236672").empty());
    CHECK(has(find("passport no G5236672"), PiiType::Passport, "G5236672"));
}

TEST(keywords_must_be_whole_words) {
    // "pan" inside "company", "cc" inside "account" must not count as context.
    CHECK(find("company VLA7529170").empty());
}

TEST(hindi_keywords_and_devanagari_digits) {
    const std::string text = "आधार संख्या: ७२६० १८१५ ९०८२";
    auto f = find(text);
    CHECK_EQ(f.size(), std::size_t(1));
    CHECK(f.size() == 1 && f[0].type == PiiType::Aadhaar);
    CHECK(f.size() == 1 && f[0].value == "७२६० १८१५ ९०८२");  // offsets map back to the original
    CHECK(f.size() == 1 && f[0].context);
    CHECK(has(find("मोबाइल नंबर 9876543210"), PiiType::Phone, "9876543210"));
}

TEST(lowercase_pan_scores_lower) {
    CHECK(find("jnepr1948v").empty());
    CHECK(has(find("pan jnepr1948v"), PiiType::Pan, "jnepr1948v"));
}

TEST(label_types_for_csv_headers) {
    const uint32_t m = det().label_types("customer_mobile");
    CHECK(m & (1u << static_cast<int>(PiiType::Phone)));
    CHECK(det().label_types("company_name") == 0);
}
