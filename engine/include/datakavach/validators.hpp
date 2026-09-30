#pragma once
// Validators: decide whether a candidate string can really be an Indian identifier.
// Most Indian IDs have a fixed structure, and some carry a check digit. Checking them is what
// separates "a random 12-digit number" from "an Aadhaar number".
#include <string_view>

namespace dk::validate {

// Verhoeff checksum over a string of digits (the scheme used by Aadhaar).
// Detects every single-digit error and every swap of two neighbouring digits.
bool verhoeff(std::string_view digits);

// Luhn (mod 10) checksum, used by payment cards.
bool luhn(std::string_view digits);

// Aadhaar: 12 digits, first digit 2-9, valid Verhoeff check digit, not all the same digit.
bool aadhaar(std::string_view digits);

// PAN: AAAAA9999A, 4th letter = holder type (P person, C company, H HUF, F firm, ...).
bool pan(std::string_view s);

// GSTIN checksum character (mod 36). Returns '\0' if the first 14 characters are not [0-9A-Z].
char gstin_check_char(std::string_view first14);

// GSTIN structure: 2-digit state code + PAN + entity number + 'Z' + check character.
bool gstin_structure(std::string_view s);
// Structure AND checksum.
bool gstin(std::string_view s);

// Card: 13-19 digits, known network prefix (Visa, Mastercard, RuPay, Amex, ...), valid Luhn.
bool card(std::string_view digits);

// Indian mobile: 10 digits starting with 6, 7, 8 or 9 (not all the same digit).
bool mobile10(std::string_view digits);

// IFSC: 4 letters (bank), '0', 6 letters or digits (branch).
bool ifsc(std::string_view s);

// EPIC (voter ID): 3 letters + 7 digits.
bool voter_id(std::string_view s);

// Indian passport number: 1 letter + 7 digits, 2nd and last digits non-zero.
bool passport(std::string_view s);

// Email: local@domain.tld with a plausible TLD.
bool email(std::string_view s);

// Known UPI handles (the part after '@'), e.g. "okaxis", "ybl", "paytm".
bool known_upi_handle(std::string_view handle);

}  // namespace dk::validate
