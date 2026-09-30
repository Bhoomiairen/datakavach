#pragma once
// The detector: finds Indian personal data in a text and scores how sure it is.
//
// Pipeline (one pass for each step, all linear in the text length):
//   1. normalise   Indian-script digits (Devanagari, Bengali, Tamil, ...) -> 0-9, keeping a map
//                  back to the original byte offsets
//   2. tokenise    maximal runs of letters/digits; digit groups joined by one space or hyphen
//   3. candidates  every shape that could be an ID; validated with checksums / structure rules
//   4. context     Aho-Corasick finds keywords ("aadhaar", "mobile", "आधार", "order no"...);
//                  a keyword of the right type nearby raises the score, "order"/"invoice" lowers it
//   5. resolve     keep the best non-overlapping findings above the threshold
#include <cstdint>
#include <string_view>
#include <vector>

#include "datakavach/aho_corasick.hpp"
#include "datakavach/types.hpp"

namespace dk {

class Detector {
public:
    Detector();

    // Findings with score >= threshold, non-overlapping, sorted by start offset.
    std::vector<Finding> detect(std::string_view text, double threshold = 0.5) const;

    // All scored candidates (overlapping, no threshold). Used by the scanner, which adds
    // CSV-header context before resolving.
    std::vector<Finding> candidates(std::string_view text) const;

    // Keep the best non-overlapping candidates with score >= threshold, sorted by start.
    static std::vector<Finding> resolve(std::vector<Finding> candidates, double threshold);

    // Bitmask (1 << type) of the PII types whose keywords appear in `label`, e.g. a CSV column
    // header "customer_mobile" -> PHONE. kNegativeLabel is set for labels like "order_ref".
    uint32_t label_types(std::string_view label) const;
    static constexpr uint32_t kNegativeLabel = 1u << 31;

    // Number-shaped types (Aadhaar, phone, card, account), where "order"/"invoice" lowers the score.
    static bool numeric_type(PiiType t);
    static constexpr double kNegativePenalty = 0.35;

    // How much a matching keyword raises the score of a finding of this type.
    static double context_boost(PiiType t);

private:
    AhoCorasick keywords_;
};

}  // namespace dk
