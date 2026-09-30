# How DataKavach works (and how to explain it in an interview)

This is the "explain it to me" version: what every part does, why it was built that way, and the
questions an interviewer is likely to ask. Read it with the code open.

---

## The 30-second pitch

> "DataKavach is a C++ engine that finds and masks Indian personal data — Aadhaar, PAN, GSTIN,
> UPI IDs, phone and bank details — in company files and in AI prompts, to help with India's DPDP
> Act. It validates matches with the real checksums, like Verhoeff for Aadhaar, uses an
> Aho-Corasick automaton to read context words in one pass, and scans folders on every core.
> I benchmarked it against Microsoft Presidio: about equal accuracy on a held-out hard test set,
> more than 2,000 times faster, and it covers UPI, IFSC and bank accounts, which Presidio doesn't."

---

## The pipeline, step by step

Example input: `Order ID 726018159082. Aadhaar: ७२६० १८१५ ९०८२, mob +91 9301947774`

### 1. Normalise (`detector.cpp → normalize`)

Indian scripts have their own digits: `७२६०` is Devanagari for `7260`. The engine copies the
text, replacing every Indian-script digit with its ASCII digit, and keeps a **map from each new byte
back to the original byte offset**. Later, when a finding is reported, its position is translated
back, so masking replaces exactly the original characters.

- In UTF-8 every Devanagari digit is 3 bytes (`E0 A5 A6`–`E0 A5 AF`). In all nine Indian script
  blocks the digits sit at the same offset (0x66–0x6F), so one check covers Devanagari, Bengali,
  Gurmukhi, Gujarati, Odia, Tamil, Telugu, Kannada and Malayalam.
- If the text has no such digits (the usual case), no copy or map is made at all.

### 2. Tokenise (`tokenize`, `add_digit_candidates`)

The text is split into runs of letters/digits. Digit runs separated by **exactly one** space or
hyphen are joined into a *group sequence*: `98765 43210` → `["98765", "43210"]`.

The trick: **every contiguous sub-sequence** of groups is tried. For
`9876543210 7260 1815 9082` the groups are `[9876543210, 7260, 1815, 9082]`; trying all
sub-sequences finds the phone (group 1) and the Aadhaar (groups 2–4) separately, even though they
are written right next to each other. A sub-sequence stops growing at 19 digits (the longest card),
so this stays linear.

### 3. Validate (`validators.cpp`)

Each candidate shape is checked against the real rules:

| ID | Rule | Why it matters |
|---|---|---|
| Aadhaar | 12 digits, starts 2-9, **Verhoeff** check digit | Catches every single-digit typo and every swap of two neighbouring digits. Only 1 in 10 random numbers passes. |
| GSTIN | state code 01-38 (or 97/99) + a valid PAN + entity number + `Z` + **mod-36** check char | A GSTIN contains the business's PAN, so it is validated twice. |
| Card | known network prefix + **Luhn** | Luhn is a mod-10 checksum where every second digit is doubled. |
| PAN | `AAAAA9999A`, 4th letter ∈ {P, C, H, F, A, T, B, L, J, G, K} | The 4th letter says who holds it: P = person, C = company… |

**Verhoeff in one breath:** it uses the multiplication table of the *dihedral group D5* (the
symmetries of a pentagon), which is not commutative — that is exactly why it catches swapped
digits, which a simple sum (like Luhn) can miss. The code is three small lookup tables and a loop.

### 4. Context (`aho_corasick.cpp`, `ContextIndex`)

Around 110 keywords, in English, Hinglish and Hindi, are matched in **one pass** with an
Aho-Corasick automaton:

- positive keywords per type: `aadhaar`, `uid`, `आधार` → Aadhaar; `mob no`, `whatsapp`,
  `मोबाइल` → phone; `a/c`, `account no` → bank account…
- negative keywords: `order id`, `invoice`, `utr`, `pnr`, `imei`, `barcode`… → "this number is
  probably a business reference".

**How Aho-Corasick works:** build a trie of all keywords. For each trie node, compute a *failure
link*: the longest proper suffix of that node's string that is also in the trie (the same idea as
KMP's prefix function, but for many patterns). Then turn it into a full state machine (DFA) so that
each input byte is **one table lookup**. Total cost: O(text + matches), no matter how many keywords.

ASCII keywords must stand alone as words, so `pan` doesn't match inside `company`.

Keyword positions are stored in sorted arrays, so "is there an Aadhaar keyword within 48 bytes
before this number?" is a **binary search**.

**Nearest keyword wins.** In `...linked to the wrong account. Order ID 2679...`, both `account`
(bank) and `Order ID` (negative) appear before the number. The engine compares distances: the
nearest keyword decides. This one rule removed 59 false bank-account alarms in testing.

### 5. Score and resolve (`Detector::resolve`)

Each candidate gets a score:

```
score = base (how distinctive the shape is)
      + context boost   (a keyword for this type is nearest)     e.g. +0.3
      − penalty         (a business word is nearest)             −0.35
      − 0.35            (ID-shaped but typed in lower case)
```

Bases: GSTIN with valid checksum 0.95, email 0.95, PAN 0.8, Aadhaar 0.55, bare phone 0.5, voter ID
0.35, passport 0.3, bank account 0.2. So a voter ID or bank account number is **only** reported
when a keyword is next to it — on its own, the shape is too common.

Then **greedy interval selection**: sort by score (then length), and accept a candidate only if it
doesn't overlap one already accepted (checked with an ordered set, O(log n)). That's how
`9876543210@ybl` becomes one UPI ID instead of a UPI ID *and* a phone number, and why the PAN inside
a GSTIN isn't reported twice.

### 6. Scanning folders (`scanner.cpp`)

- Collect text-like files (skips `.git`, `node_modules`, binaries detected by a NUL byte).
- **Thread pool with an atomic counter**: each worker takes the next file index (`next++`), so fast
  and slow files balance out automatically.
- Files are read in 8 MB chunks that always **end at a newline**, so a number is never split between
  chunks and memory stays flat for huge files.
- **CSV/TSV:** each finding is assigned to a column (counting commas outside quotes). The **column
  header is context too**: a bare 10-digit number under `customer_mobile` gets the phone boost, and
  a checksum-valid 12-digit number under `last_order_ref` gets the negative penalty.
- The report stores **counts and partially masked samples only**, never raw values.

### 7. Masking and the AI gateway (`masker.cpp`, `web/app/api/gateway/route.js`)

- **Token mode** gives each distinct value a numbered token: `[PHONE_1]`, `[PHONE_2]`. The same value
  always gets the same token. The mapping (the *vault*) stays on the server.
- The gateway masks **all user messages of the conversation in one engine call** (joined by a
  separator line). Because numbering follows first appearance, `[AADHAAR_1]` in turn 3 is the same
  Aadhaar as in turn 1.
- The model is told to keep placeholders as they are. Its answer is un-masked before you see it, so
  the real values never leave the machine.

---

## Design decisions (and the honest trade-offs)

**Why C++?** The scanner runs over whole file shares. The hot loop is byte-by-byte state-machine work,
where C++ is ideal: no garbage collector, tight memory, and threads without a global lock. Result:
~30 MB/s per core on dense test text, versus 0.013 MB/s for Presidio (which runs a spaCy model on
every sentence).

**Why rules and not machine learning?** Indian IDs have *exact* structure and checksums. A checksum
is a stronger signal than any model, and it is explainable: "flagged because the Verhoeff check
digit is valid and the word 'aadhaar' is 12 bytes before it." ML would help for names and
addresses, which this engine deliberately does not do.

**Why two test sets?** I wrote the rules and the standard test data, so a 0.998 F1 there mostly
proves the rules match my own templates. The hard set was written *after* freezing the rules and is
never used for tuning. Its 0.71 F1 is the honest number, and its failures tell me what to build next.

**Why is Presidio's threshold tuned but mine isn't?** To make the comparison fair to Presidio: its
scores mean different things from mine, so it gets its best threshold on each set; DataKavach uses its
default 0.5 everywhere.

---

## Questions you should be ready for

1. **What is the time complexity of a scan?** Linear in the text size: normalising, tokenising and
   the Aho-Corasick pass are all O(n); each candidate costs a few O(log k) binary searches; resolving
   is O(c log c) for c candidates.
2. **Why does Verhoeff catch swapped digits when Luhn sometimes doesn't?** Verhoeff multiplies in a
   non-commutative group (D5), so a·b ≠ b·a; Luhn is a sum, and it misses the swap 09 ↔ 90.
3. **What is a failure link?** For a trie node spelling string s, the node for the longest proper
   suffix of s that is also a prefix of some keyword. It says where to continue after a mismatch
   without re-reading input.
4. **How do you avoid flagging order numbers?** Checksum first, then negative keywords (nearest
   keyword wins), then CSV column headers. The remaining false alarms are business words not in the
   list — see the hard set.
5. **How is the work split across threads, and is it thread-safe?** One read-only `Detector` is
   shared; each thread writes only its own slot in the results vector; the only shared mutable state
   is one `std::atomic<size_t>` counter.
6. **What happens with a 5 GB log file?** It is streamed in 8 MB chunks that end on a newline;
   memory stays at a few tens of MB per thread.
7. **How do you make sure raw values don't leak?** Reports hold partially masked samples;
   `--show-values` must be passed explicitly; uploaded files are deleted right after the scan; the
   AI model sees only tokens.
8. **What would you improve next?** OCR-noise handling and line-broken numbers (the two biggest
   hard-set failures), measured on a *new* frozen set, so the old one isn't overfitted.
9. **Is it DPDP-compliant?** It helps a company *map* and *mask* personal data, which DPDP
   requires. Compliance is a process (consent, notices, retention), not a tool.
10. **Why not just use regex?** Regex finds shapes; most of the value here is deciding which shapes
    are real (checksums, context, column headers) — and a hand-written tokenizer plus one automaton
    is much faster than dozens of regexes over the same text.

---

## Where each thing lives

| Concept | File |
|---|---|
| Aho-Corasick automaton | `engine/src/aho_corasick.cpp` |
| Verhoeff, Luhn, GSTIN mod-36, PAN rules | `engine/src/validators.cpp` |
| Normalisation, tokens, context, scoring, resolve | `engine/src/detector.cpp` |
| Partial / redact / token masking, unmask | `engine/src/masker.cpp` |
| Thread pool, chunked reading, CSV columns, JSON report | `engine/src/scanner.cpp` |
| Unit tests | `engine/tests/` |
| Synthetic data (standard / hard) | `data/generate.py`, `data/hard.py` |
| Precision / recall / F1, Presidio baseline | `eval/evaluate.py` |
| Engine bridge used by the dashboard | `web/lib/engine.js` |
| AI gateway (mask → LLM → unmask) | `web/app/api/gateway/route.js` |
