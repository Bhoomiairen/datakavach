// What each kind of personal data is, how it is checked, and its highlighter colour.
export const TYPES = {
  AADHAAR: { label: "Aadhaar", color: "#f6c453", check: "12 digits, Verhoeff check digit", risk: "high" },
  PAN: { label: "PAN", color: "#a8d58f", check: "AAAAA9999A, 4th letter = holder type", risk: "high" },
  GSTIN: { label: "GSTIN", color: "#8fd3c8", check: "state code + PAN + mod-36 check character", risk: "medium" },
  CARD: { label: "Card", color: "#f4a3b4", check: "network prefix + Luhn checksum", risk: "high" },
  PHONE: { label: "Phone", color: "#f7e66b", check: "10 digits starting 6-9, +91 / 0 prefix", risk: "medium" },
  BANK_ACCOUNT: { label: "Bank a/c", color: "#c9b6f2", check: "9-18 digits, only next to 'a/c' / 'account'", risk: "high" },
  IFSC: { label: "IFSC", color: "#a9cdf5", check: "4 letters + 0 + 6 characters", risk: "low" },
  UPI: { label: "UPI ID", color: "#f8b98a", check: "name@handle, known UPI handles", risk: "medium" },
  EMAIL: { label: "Email", color: "#9fd6f0", check: "local@domain.tld", risk: "medium" },
  VOTER_ID: { label: "Voter ID", color: "#f2a58f", check: "3 letters + 7 digits, needs context", risk: "high" },
  PASSPORT: { label: "Passport", color: "#b4e3b0", check: "letter + 7 digits, needs context", risk: "high" },
};

export const TYPE_ORDER = Object.keys(TYPES);

export function typeLabel(t) {
  return TYPES[t]?.label ?? t;
}

export function why(f) {
  const bits = [TYPES[f.type]?.check ?? "shape matched"];
  if (f.context) bits.push("keyword nearby");
  if (f.negative) bits.push("business-number word nearby (score lowered)");
  return bits.join(" · ");
}
