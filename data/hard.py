"""The HARD test set: written after the engine's rules were frozen, and never used to tune them.

The standard set (generate.py) uses the same phrasing the rules were designed around, so its
scores are an upper bound. This set deliberately uses what the rules were NOT designed for:
OCR noise, line breaks inside numbers, obfuscated emails, keywords the engine doesn't know
("folio", "consignment", "PP no"), lower-case IDs, unusual phone formats, and business numbers
behind words that are not in the negative list. Expect clearly lower scores here - that is the
point: it shows where the engine breaks.

    python data/hard.py   ->  data/eval/hard.jsonl
"""
from __future__ import annotations

import json
import random
from pathlib import Path

import idgen
from generate import CITIES, COMPANIES, DEVANAGARI, Doc, email_of, name

ROOT = Path(__file__).resolve().parent
TAMIL = str.maketrans("0123456789", "௦௧௨௩௪௫௬௭௮௯")


def ocr_noise(rng):
    d = Doc()
    a = idgen.aadhaar(rng)
    noisy = a[:4] + " " + a[4:8] + " " + a[8:].replace("0", "O", 1) if "0" in a[8:] else a[:4] + "  " + a[4:8] + "  " + a[8:]
    d.t("Scanned KYC -> Aadhaar ").pii("AADHAAR", noisy).t(" (auto-extracted)")
    return d


def line_break(rng):
    d = Doc()
    a = idgen.aadhaar(rng)
    d.t("Aadhaar:\n").pii("AADHAAR", f"{a[:4]}\n{a[4:8]} {a[8:]}").t("\nStatus: verified")
    return d


def odd_phone_formats(rng):
    d = Doc()
    m = idgen.mobile(rng)
    fmt = rng.choice([f"(+91) {m[:5]}-{m[5:]}", f"+91 ({m[:3]}) {m[3:6]}-{m[6:]}", f"{m[:5]}.{m[5:]}",
                      f"+91 {m[:2]} {m[2:6]} {m[6:]}", f"91-{m}"])
    d.t(rng.choice(["Reach me: ", "Mob. ", "Tel ", "Number - "])).pii("PHONE", fmt)
    return d


def obfuscated_email(rng):
    d, p = Doc(), name(rng)
    e = email_of(rng, p)
    user, dom = e.split("@")
    d.t("write to ").pii("EMAIL", f"{user} [at] {dom.replace('.', ' [dot] ')}").t(" for the refund")
    return d


def unknown_contexts(rng):
    """Business numbers behind words that are not in the negative list."""
    d = Doc()
    d.t(rng.choice([f"Consignment no {idgen.aadhaar(rng)} dispatched",
                    f"Folio {idgen.mobile(rng)} credited with dividend",
                    f"Customer ID {idgen.mobile(rng)} upgraded to premium",
                    f"Ack. {idgen.aadhaar(rng)} generated for your return",
                    f"Meter {idgen.mobile(rng)} reading 4521 units",
                    f"Complaint {idgen.aadhaar(rng)} escalated to L2"]))
    return d


def lower_case_ids(rng):
    d = Doc()
    kind = rng.random()
    if kind < 0.5:
        d.t("my details: ").pii("PAN", idgen.pan(rng).lower()).t(", pls update")
    else:
        d.t("reg no ").pii("GSTIN", idgen.gstin(rng).lower())
    return d


def missing_keywords(rng):
    """Real IDs with context words the engine does not know."""
    d = Doc()
    kind = rng.random()
    if kind < 0.35:
        d.t("PP no ").pii("PASSPORT", idgen.passport(rng)).t(", expires 2031")
    elif kind < 0.65:
        d.t("EPIC card ").pii("VOTER_ID", idgen.voter_id(rng))
    else:
        d.t(f"pay {name(rng).split()[0]} at ").pii("UPI", f"{name(rng).split()[0].lower()}@{rng.choice(['kvb', 'dlb', 'tmb', 'csbpay'])}")
    return d


def other_scripts(rng):
    d = Doc()
    m = idgen.mobile(rng)
    if rng.random() < 0.5:
        d.t("தொலைபேசி எண் ").pii("PHONE", m.translate(TAMIL))
    else:
        a = idgen.aadhaar(rng)
        d.t("आधार संख्या ").pii("AADHAAR", f"{a[:4]} {a[4:8]} {a[8:]}".translate(DEVANAGARI)).t(" है")
    return d


def realistic_mix(rng):
    d, p = Doc(), name(rng)
    d.t(f"From: {p}\nSubject: Re: salary account\n\nHi, the new account is with {rng.choice(['HDFC', 'SBI'])}"
        " - acc ").pii("BANK_ACCOUNT", idgen.bank_account(rng)).t(" ifsc ").pii("IFSC", idgen.ifsc(rng))
    d.t(". My pan is ").pii("PAN", idgen.pan(rng)).t(f". Thanks, {p.split()[0]}\n")
    d.t(f"Sent from my phone | {rng.choice(CITIES)} | Ref {rng.randint(10**9, 10**10 - 1)}")
    return d


def invoice_with_gst(rng):
    d = Doc()
    d.t(f"TAX INVOICE\n{rng.choice(COMPANIES)}\nGST No.: ").pii("GSTIN", idgen.gstin(rng))
    d.t(f"\nHSN 1006 | Qty 20 | Rate 45.00 | Total 900.00\nBill to GSTIN ").pii("GSTIN", idgen.gstin(rng))
    d.t(f"\nPO {rng.randint(10**9, 10**10 - 1)}")
    return d


TEMPLATES = [ocr_noise, line_break, odd_phone_formats, obfuscated_email, unknown_contexts, lower_case_ids,
             missing_keywords, other_scripts, realistic_mix, invoice_with_gst]


def main():
    rng = random.Random(4242)
    docs = []
    for i in range(600):
        f = TEMPLATES[i % len(TEMPLATES)]
        d = f(rng)
        docs.append({"id": i, "kind": f.__name__, "text": d.text(), "spans": d.spans})
    out = ROOT / "eval" / "hard.jsonl"
    with open(out, "w", encoding="utf-8") as fh:
        for d in docs:
            fh.write(json.dumps(d, ensure_ascii=False) + "\n")
    print(f"hard set: {len(docs)} documents, {sum(len(d['spans']) for d in docs)} labelled values -> {out.relative_to(ROOT.parent)}")


if __name__ == "__main__":
    main()
