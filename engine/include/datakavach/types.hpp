#pragma once
// Core types shared by every part of the engine.
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace dk {

enum class PiiType : int {
    Aadhaar = 0,
    Pan,
    Gstin,
    Card,
    Phone,
    BankAccount,
    Ifsc,
    Upi,
    Email,
    VoterId,
    Passport,
    Count
};

constexpr int kTypeCount = static_cast<int>(PiiType::Count);

constexpr std::array<std::string_view, kTypeCount> kTypeNames = {
    "AADHAAR", "PAN", "GSTIN", "CARD", "PHONE", "BANK_ACCOUNT",
    "IFSC", "UPI", "EMAIL", "VOTER_ID", "PASSPORT",
};

inline std::string_view type_name(PiiType t) { return kTypeNames[static_cast<int>(t)]; }
bool type_from_name(std::string_view name, PiiType& out);

// One piece of personal data found in a text.
// start/end are BYTE offsets into the original (UTF-8) text, end exclusive.
struct Finding {
    PiiType type{};
    std::size_t start = 0;
    std::size_t end = 0;
    double score = 0.0;         // 0..1 confidence
    bool context = false;       // a keyword such as "aadhaar" or "mobile" was nearby
    bool negative = false;      // a word such as "order" or "invoice" was nearer (score lowered)
    std::string value;          // the matched text, exactly as written
};

}  // namespace dk
