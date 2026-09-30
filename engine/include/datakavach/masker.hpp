#pragma once
// Masking: rewrite a text so the personal data in it is hidden.
//   redact   "Aadhaar 2345 6789 0124"  ->  "Aadhaar [AADHAAR]"
//   partial  "Aadhaar 2345 6789 0124"  ->  "Aadhaar XXXX XXXX 0124"   (keeps format + last 4)
//   token    "Aadhaar 2345 6789 0124"  ->  "Aadhaar [AADHAAR_1]"      (reversible with the vault)
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "datakavach/types.hpp"

namespace dk {

enum class MaskMode { Redact, Partial, Token };

bool mask_mode_from_name(std::string_view name, MaskMode& out);

struct MaskResult {
    std::string text;
    // token -> original value (token mode only). The same value always gets the same token,
    // so "call 98765 43210" twice becomes [PHONE_1] twice.
    std::map<std::string, std::string> vault;
};

// `findings` must be non-overlapping and sorted by start (as Detector::detect returns them).
MaskResult mask(std::string_view text, const std::vector<Finding>& findings, MaskMode mode);

// The partially masked form of one value, e.g. "XXXXXX234F" for a PAN.
std::string partial_mask(PiiType type, std::string_view value);

// Replace tokens such as [PHONE_1] with their original values (for AI gateway answers).
std::string unmask(std::string_view text, const std::map<std::string, std::string>& vault);

}  // namespace dk
