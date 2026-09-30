"""Builds DataKavach's synthetic test data. Nothing here is real personal data.

    python data/generate.py                 # labelled eval set + demo company folder
    python data/generate.py --bench 200     # also a ~200 MB corpus for speed tests

Outputs
  data/eval/docs.jsonl         2,000 labelled documents: {"id", "kind", "text", "spans": [[start, end, type]]}
                               (character offsets). Includes hard negatives: order numbers that pass
                               the Aadhaar checksum, PNRs, IMEIs, amounts, request IDs...
  data/sample_company/         a messy "company file share" (CSVs, logs, JSON, tickets) for the demo
  data/bench/                  (only with --bench) large log/ticket files for throughput tests
"""
from __future__ import annotations

import argparse
import json
import random
import string
from pathlib import Path

import idgen

ROOT = Path(__file__).resolve().parent

FIRST = ["Priya", "Rahul", "Ananya", "Arjun", "Sneha", "Vikram", "Kavya", "Rohan", "Meera", "Aditya",
         "Pooja", "Karthik", "Divya", "Siddharth", "Nisha", "Amit", "Lakshmi", "Suresh", "Fatima", "Imran",
         "Gurpreet", "Harpreet", "Anjali", "Manoj", "Deepa", "Ravi", "Sunita", "Arnav", "Ishita", "Tanvi",
         "Nikhil", "Shreya", "Farhan", "Zoya", "Joseph", "Mary", "Bhoomi", "Kiran", "Ayesha", "Tenzin"]
LAST = ["Sharma", "Verma", "Iyer", "Nair", "Reddy", "Patel", "Shah", "Gupta", "Singh", "Kaur",
        "Menon", "Rao", "Das", "Chatterjee", "Banerjee", "Khan", "Qureshi", "Joshi", "Kulkarni", "Deshpande",
        "Pillai", "Mishra", "Yadav", "Chauhan", "Fernandes", "D'Souza", "Mehta", "Agarwal", "Bose", "Naidu"]
CITIES = ["Mumbai", "Pune", "Bengaluru", "Chennai", "Hyderabad", "Kolkata", "Delhi", "Ahmedabad", "Jaipur",
          "Lucknow", "Indore", "Kochi", "Bhopal", "Nagpur", "Surat", "Guwahati", "Chandigarh", "Patna"]
COMPANIES = ["Shree Traders", "Vardhman Textiles", "Ganesh Enterprises", "Sai Logistics", "Om Agro Foods",
             "Kaveri Electricals", "Tulsi Pharma", "Annapurna Caterers", "Bharat Hardware", "Nilgiri Tea Co"]
BANKS = ["SBI", "HDFC Bank", "ICICI Bank", "Axis Bank", "PNB", "Kotak", "Bank of Baroda", "Canara Bank"]
DEVANAGARI = str.maketrans("0123456789", "०१२३४५६७८९")


def name(rng):
    return f"{rng.choice(FIRST)} {rng.choice(LAST)}"


def email_of(rng, person):
    first, last = person.lower().replace("'", "").split()[:2]
    user = rng.choice([f"{first}.{last}", f"{first}{rng.randint(1, 99)}", f"{first[0]}{last}", f"{first}_{last}"])
    return f"{user}@{rng.choice(['gmail.com', 'yahoo.co.in', 'outlook.com', 'rediffmail.com', 'company.in'])}"


# ------------------------------------------------------------------ formatting variants

def fmt_aadhaar(rng, a):
    return rng.choice([f"{a[:4]} {a[4:8]} {a[8:]}", a, f"{a[:4]}-{a[4:8]}-{a[8:]}", f"{a[:4]} {a[4:8]} {a[8:]}"])


def fmt_phone(rng, m):
    return rng.choice([m, m, f"+91 {m[:5]} {m[5:]}", f"+91-{m}", f"0{m[:5]} {m[5:]}", f"{m[:5]}-{m[5:]}",
                       f"+91{m}", f"{m[:5]} {m[5:]}"])


def fmt_card(rng, c):
    return rng.choice([f"{c[:4]} {c[4:8]} {c[8:12]} {c[12:]}", c, f"{c[:4]}-{c[4:8]}-{c[8:12]}-{c[12:]}"])


class Doc:
    """Builds a text while remembering where each piece of personal data was placed."""

    def __init__(self):
        self.parts: list[str] = []
        self.spans: list[list] = []
        self.length = 0

    def t(self, s: str):
        self.parts.append(s)
        self.length += len(s)
        return self

    def pii(self, kind: str, s: str):
        self.spans.append([self.length, self.length + len(s), kind])
        return self.t(s)

    def text(self):
        return "".join(self.parts)


# ------------------------------------------------------------------ document templates

def ticket_en(rng):
    d, p = Doc(), name(rng)
    d.t(rng.choice(["Hi team, ", "Hello, ", "Dear support, ", ""])).t(f"I'm {p} from {rng.choice(CITIES)}. ")
    if rng.random() < 0.7:
        d.t(rng.choice(["My Aadhaar ", "My aadhaar number ", "Aadhaar no. ", "UID "]))
        d.pii("AADHAAR", fmt_aadhaar(rng, idgen.aadhaar(rng))).t(" is linked to the wrong account. ")
    if rng.random() < 0.8:
        d.t(rng.choice(["Please call me on ", "You can reach me at ", "My mobile is ", "Contact: "]))
        d.pii("PHONE", fmt_phone(rng, idgen.mobile(rng))).t(". ")
    if rng.random() < 0.4:
        d.t("Or email ").pii("EMAIL", email_of(rng, p)).t(".")
    if rng.random() < 0.3:
        d.t(f" Order ID {rng.randint(10**11, 10**12 - 1)} is still pending.")
    return d


def ticket_hinglish(rng):
    d = Doc()
    d.t(rng.choice(["Sir ", "Hello sir, ", "Madam ", ""]))
    d.t(rng.choice(["mera aadhar ", "mera aadhaar number ", "aadhar card no "]))
    d.pii("AADHAAR", fmt_aadhaar(rng, idgen.aadhaar(rng)))
    d.t(rng.choice([" update nahi ho raha. ", " galat link ho gaya hai. ", " pe OTP nahi aa raha. "]))
    d.t(rng.choice(["Mera mob no ", "Mujhe call karo ", "whatsapp pe msg karo "]))
    d.pii("PHONE", fmt_phone(rng, idgen.mobile(rng)))
    if rng.random() < 0.5:
        d.t(". Paise ").pii("UPI", idgen.upi(rng, name(rng))).t(" pe bhej diye the")
    d.t(rng.choice([". Plz help", " jaldi solve karo", "."]))
    return d


def ticket_hindi(rng):
    d = Doc()
    d.t("नमस्ते, मेरा ")
    a = idgen.aadhaar(rng)
    af = fmt_aadhaar(rng, a)
    d.t("आधार नंबर ").pii("AADHAAR", af.translate(DEVANAGARI) if rng.random() < 0.6 else af)
    d.t(" है। कृपया मेरे मोबाइल ")
    m = fmt_phone(rng, idgen.mobile(rng))
    d.pii("PHONE", m.translate(DEVANAGARI) if rng.random() < 0.4 else m).t(" पर संपर्क करें।")
    if rng.random() < 0.4:
        d.t(" पैन: ").pii("PAN", idgen.pan(rng))
    return d


def kyc_form(rng):
    d, p = Doc(), name(rng)
    d.t(f"KYC FORM\nName: {p}\nPAN: ").pii("PAN", idgen.pan(rng))
    d.t("\nAadhaar No: ").pii("AADHAAR", fmt_aadhaar(rng, idgen.aadhaar(rng)))
    d.t(f"\nDOB: {rng.randint(1, 28):02d}/{rng.randint(1, 12):02d}/{rng.randint(1965, 2004)}")
    d.t(f"\nAddress: {rng.randint(1, 400)}, {rng.choice(['MG Road', 'Station Road', 'Park Street', 'Linking Road'])}, "
        f"{rng.choice(CITIES)} {rng.randint(110001, 855999)}")
    d.t("\nMobile: ").pii("PHONE", fmt_phone(rng, idgen.mobile(rng)))
    d.t("\nEmail: ").pii("EMAIL", email_of(rng, p))
    if rng.random() < 0.5:
        d.t("\nPassport No: ").pii("PASSPORT", idgen.passport(rng))
    if rng.random() < 0.5:
        d.t("\nVoter ID: ").pii("VOTER_ID", idgen.voter_id(rng))
    return d


def vendor_note(rng):
    d = Doc()
    d.t(f"Vendor: {rng.choice(COMPANIES)}\nGSTIN: ").pii("GSTIN", idgen.gstin(rng))
    d.t(f"\nBank: {rng.choice(BANKS)}, A/c no ").pii("BANK_ACCOUNT", idgen.bank_account(rng))
    d.t(", IFSC ").pii("IFSC", idgen.ifsc(rng))
    d.t(f"\nInvoice INV-{rng.randint(1000, 99999)} dated {rng.randint(1, 28)}/0{rng.randint(1, 9)}/2026 "
        f"for Rs {rng.randint(1000, 999999)}.{rng.randint(10, 99)}")
    return d


def payment_chat(rng):
    d = Doc()
    d.t(f"paid {rng.randint(50, 5000)} to ").pii("UPI", idgen.upi(rng, name(rng)))
    d.t(rng.choice([" via gpay", " on phonepe", " using BHIM", ""]))
    txn = idgen.aadhaar(rng) if rng.random() < 0.5 else str(rng.randint(10**11, 10**12 - 1))
    d.t(f", UTR {txn}")  # a 12-digit bank reference, sometimes checksum-valid: NOT an Aadhaar
    if rng.random() < 0.4:
        d.t(". Card used: ").pii("CARD", fmt_card(rng, idgen.card(rng)))
    return d


def app_log(rng):
    d = Doc()
    ts = f"2026-0{rng.randint(1, 9)}-{rng.randint(10, 28)}T{rng.randint(10, 23)}:{rng.randint(10, 59)}:{rng.randint(10, 59)}Z"
    d.t(f"{ts} INFO auth user_id={rng.randint(1000, 99999)} phone=").pii("PHONE", idgen.mobile(rng))
    d.t(f" action=otp_sent ip=10.{rng.randint(0, 255)}.{rng.randint(0, 255)}.{rng.randint(0, 255)}")
    d.t(f" order_id={rng.randint(10**11, 10**12 - 1)} latency_ms={rng.randint(3, 900)}")
    if rng.random() < 0.3:
        d.t(" email=").pii("EMAIL", email_of(rng, name(rng)))
    return d


def json_record(rng):
    d, p = Doc(), name(rng)
    d.t('{"customer": "' + p + '", "mobile": "').pii("PHONE", idgen.mobile(rng))
    d.t('", "pan": "').pii("PAN", idgen.pan(rng))
    if rng.random() < 0.5:
        d.t('", "card": "').pii("CARD", idgen.card(rng))
    d.t(f'", "city": "{rng.choice(CITIES)}", "orders": {rng.randint(1, 40)}}}')
    return d


def travel(rng):
    d, p = Doc(), name(rng)
    d.t(f"Booking for {p}. Passport number ").pii("PASSPORT", idgen.passport(rng))
    d.t(f", PNR {rng.randint(6, 9)}{rng.randint(100000000, 999999999)}")  # 10 digits, looks like a phone
    if rng.random() < 0.5:
        d.t(". ID proof: voter ID ").pii("VOTER_ID", idgen.voter_id(rng))
    return d


def negatives(rng):
    """Business text full of numbers that are NOT personal data."""
    d = Doc()
    order12 = idgen.aadhaar(rng)  # passes the Aadhaar checksum on purpose
    d.t(rng.choice([
        f"Order ID {order12} shipped via Bluedart AWB {rng.randint(10**10, 10**11 - 1)}. Total Rs {rng.randint(100, 99999)}.{rng.randint(10, 99)}",
        f"Invoice no {order12} | qty {rng.randint(1, 50)} | amount INR {rng.randint(1000, 999999)}.00",
        f"Ticket {rng.randint(10**9, 10**10 - 1)} closed. SLA 48h. Ref {rng.randint(10**11, 10**12 - 1)}",
        f"IMEI {rng.randint(10**14, 10**15 - 1)} registered on {rng.randint(1, 28)}-0{rng.randint(1, 9)}-2026",
        f"PNR {rng.randint(6, 9)}{rng.randint(100000000, 999999999)} confirmed, coach B{rng.randint(1, 9)}, berth {rng.randint(1, 72)}",
        f"Transaction {order12} settled; batch {rng.randint(100, 999)}; reference {rng.randint(10**9, 10**10 - 1)}",
        f"Serial {rng.randint(10**11, 10**12 - 1)} product SKU-{rng.randint(1000, 9999)} amount Rs {rng.randint(10**9, 10**10 - 1)}.50",
        f"Build 2026.{rng.randint(1, 12)}.{rng.randint(1, 30)} deployed at {rng.randint(10, 23)}:{rng.randint(10, 59)} by CI job {rng.randint(100000, 999999)}",
    ]))
    return d


def ambiguous(rng):
    """Honest hard cases: IDs with no context at all."""
    d = Doc()
    kind = rng.random()
    if kind < 0.4:
        d.t(rng.choice(["ping ", "try ", "or ", "backup no "])).pii("PHONE", idgen.mobile(rng)).t(" tomorrow")
    elif kind < 0.7:
        d.t(f"request_id={rng.randint(6, 9)}{rng.randint(100000000, 999999999)} status=200")  # not a phone
    else:
        d.t("id ").pii("AADHAAR", idgen.aadhaar(rng)).t(" verified")
    return d


TEMPLATES = [
    (ticket_en, 0.17), (ticket_hinglish, 0.10), (ticket_hindi, 0.08), (kyc_form, 0.12), (vendor_note, 0.10),
    (payment_chat, 0.10), (app_log, 0.10), (json_record, 0.08), (travel, 0.05), (negatives, 0.07),
    (ambiguous, 0.03),
]


def build_eval(n: int, seed: int = 2026):
    rng = random.Random(seed)
    funcs, weights = zip(*TEMPLATES)
    docs = []
    for i in range(n):
        f = rng.choices(funcs, weights)[0]
        d = f(rng)
        docs.append({"id": i, "kind": f.__name__, "text": d.text(), "spans": d.spans})
    return docs


# ------------------------------------------------------------------ demo company folder

def write_company(folder: Path, seed: int = 7):
    rng = random.Random(seed)
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "crm").mkdir(exist_ok=True)
    (folder / "hr").mkdir(exist_ok=True)
    (folder / "finance").mkdir(exist_ok=True)
    (folder / "support").mkdir(exist_ok=True)
    (folder / "logs").mkdir(exist_ok=True)
    (folder / "catalog").mkdir(exist_ok=True)

    with open(folder / "crm/customers.csv", "w") as f:
        f.write("customer_id,name,customer_mobile,email,city,pan,last_order_ref\n")
        for i in range(400):
            p = name(rng)
            f.write(f"C{10000 + i},{p},{idgen.mobile(rng)},{email_of(rng, p)},{rng.choice(CITIES)},"
                    f"{idgen.pan(rng)},{idgen.aadhaar(rng)}\n")  # last_order_ref: checksum-valid, not Aadhaar
    with open(folder / "crm/leads_export.csv", "w") as f:
        f.write("lead,source,phone,notes\n")
        for _ in range(250):
            p = name(rng)
            note = rng.choice(["interested in gold plan", f"shared aadhaar {fmt_aadhaar(rng, idgen.aadhaar(rng))} on call",
                               "call back next week", f"alt no {idgen.mobile(rng)}", "not reachable"])
            f.write(f"{p},{rng.choice(['website', 'walk-in', 'referral', 'instagram'])},{fmt_phone(rng, idgen.mobile(rng))},\"{note}\"\n")
    with open(folder / "hr/employees.csv", "w") as f:
        f.write("emp_id,name,pan,aadhaar,bank_account,ifsc,passport_no,personal_email\n")
        for i in range(120):
            p = name(rng)
            f.write(f"E{2000 + i},{p},{idgen.pan(rng)},{idgen.aadhaar(rng)},{idgen.bank_account(rng)},"
                    f"{idgen.ifsc(rng)},{idgen.passport(rng) if rng.random() < 0.5 else ''},{email_of(rng, p)}\n")
    with open(folder / "finance/vendors.tsv", "w") as f:
        f.write("vendor\tgstin\taccount_no\tifsc\tcontact_mobile\n")
        for _ in range(60):
            f.write(f"{rng.choice(COMPANIES)}\t{idgen.gstin(rng)}\t{idgen.bank_account(rng)}\t{idgen.ifsc(rng)}\t{idgen.mobile(rng)}\n")
    with open(folder / "finance/payments.jsonl", "w") as f:
        for _ in range(300):
            rec = {"utr": idgen.aadhaar(rng) if rng.random() < 0.3 else str(rng.randint(10**11, 10**12 - 1)),
                   "amount": round(rng.uniform(100, 90000), 2), "payer_vpa": idgen.upi(rng, name(rng)),
                   "status": rng.choice(["SUCCESS", "SUCCESS", "FAILED"])}
            if rng.random() < 0.2:
                rec["card"] = fmt_card(rng, idgen.card(rng))
            f.write(json.dumps(rec) + "\n")
    with open(folder / "support/tickets_2026.txt", "w", encoding="utf-8") as f:
        for i in range(250):
            d = rng.choice([ticket_en, ticket_hinglish, ticket_hindi])(rng)
            f.write(f"--- Ticket #{5000 + i} ---\n{d.text()}\n\n")
    with open(folder / "logs/auth-service.log", "w") as f:
        for _ in range(3000):
            f.write(app_log(rng).text() + "\n")
    with open(folder / "catalog/products.csv", "w") as f:  # clean file: no personal data
        f.write("sku,product,price_inr,stock,barcode\n")
        for i in range(500):
            f.write(f"SKU-{10000 + i},{rng.choice(['Tea', 'Rice', 'Soap', 'Oil', 'Atta', 'Dal'])} {rng.randint(1, 5)}kg,"
                    f"{rng.randint(40, 2000)}.00,{rng.randint(0, 900)},{rng.randint(10**12, 10**13 - 1)}\n")
    with open(folder / "README.md", "w") as f:
        f.write("# Sample company share\n\nSynthetic files for trying DataKavach. No real people.\n")


def write_bench(folder: Path, megabytes: int, seed: int = 99):
    rng = random.Random(seed)
    folder.mkdir(parents=True, exist_ok=True)
    per_file = 25 * 10**6
    files = max(1, megabytes * 10**6 // per_file)
    for k in range(files):
        with open(folder / f"part-{k:03d}.log", "w", encoding="utf-8") as f:
            written = 0
            while written < per_file:
                block = "\n".join(rng.choice([app_log, ticket_en, negatives, payment_chat, kyc_form])(rng).text()
                                  for _ in range(200)) + "\n"
                f.write(block)
                written += len(block.encode())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--docs", type=int, default=2000)
    ap.add_argument("--bench", type=int, default=0, help="also write a benchmark corpus of about N MB")
    args = ap.parse_args()

    docs = build_eval(args.docs)
    out = ROOT / "eval"
    out.mkdir(exist_ok=True)
    with open(out / "docs.jsonl", "w", encoding="utf-8") as f:
        for d in docs:
            f.write(json.dumps(d, ensure_ascii=False) + "\n")
    spans = sum(len(d["spans"]) for d in docs)
    print(f"eval set: {len(docs)} documents, {spans} labelled values -> data/eval/docs.jsonl")

    write_company(ROOT / "sample_company")
    print("demo folder -> data/sample_company/")
    if args.bench:
        write_bench(ROOT / "bench", args.bench)
        print(f"benchmark corpus (~{args.bench} MB) -> data/bench/")


if __name__ == "__main__":
    main()
