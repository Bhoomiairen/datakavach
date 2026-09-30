// Example texts for the playground. All identifiers are synthetic (they pass the checksums but
// were generated for this project).
export const SAMPLES = [
  {
    id: "kyc",
    label: "KYC form",
    text: `KYC FORM
Name: Bhoomi Airen 
PAN: JNEPR1948V
Aadhaar No: 7260 1815 9082
Mobile: +91 9301947774
Email: airenbhoomi@gmail.com
Bank: HDFC Bank, A/c no 50100234567891, IFSC HDFC0049746`,
  },
  {
    id: "hinglish",
    label: "Hinglish ticket",
    text: `Sir mera aadhar 5016 6131 8603 update nahi ho raha. Mera mob no 8123456789 hai,
whatsapp pe msg karo. Paise rahul.verma@okaxis pe bhej diye the. Plz help`,
  },
  {
    id: "hindi",
    label: "हिंदी",
    text: `नमस्ते, मेरा आधार नंबर ७२६० १८१५ ९०८२ है। कृपया मेरे मोबाइल ९८७६५ ४३२१० पर संपर्क करें।
पैन: FDSCS3518W`,
  },
  {
    id: "vendor",
    label: "Vendor invoice",
    text: `TAX INVOICE — Vardhman Textiles
GSTIN: 27AAPFU0939F1ZV
Bank: SBI, A/c no 38291047562, IFSC SBIN0004521
Invoice INV-86936 dated 20/08/2026 for Rs 48,250.00
Card used for advance: 4111 1111 1111 1111`,
  },
  {
    id: "log",
    label: "App log",
    text: `2026-03-14T10:22:31Z INFO auth user_id=83721 phone=9123456780 action=otp_sent ip=10.2.3.4
2026-03-14T10:22:35Z INFO auth user_id=83721 email=kavya.nair@outlook.com action=login
2026-03-14T10:23:02Z WARN pay order_id=726018159082 upi=9123456780@ybl status=FAILED`,
  },
  {
    id: "tricky",
    label: "Tricky numbers",
    text: `Order ID 726018159082 shipped. PNR 9225035027 confirmed.
Amount credited: 9876543210.50. Barcode 4006381333931.
(None of these are personal data — the first one even passes the Aadhaar checksum.)`,
  },
];
