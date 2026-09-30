"""Measures DataKavach on the labelled synthetic set, and compares it with Microsoft Presidio.

    python eval/evaluate.py                  # DataKavach only
    python eval/evaluate.py --presidio       # + Presidio baseline (pip install -r eval/requirements.txt)

Accuracy: a prediction counts as correct when it has the right type and overlaps a labelled value
by at least 50% (intersection over union). Precision = correct / predicted, recall = correct / labelled.
Speed: MB of text processed per second, on this machine.

Writes eval/results.json (read by the dashboard's Benchmarks page) and prints a table.
"""
from __future__ import annotations

import argparse
import json
import logging
import os
import platform
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = Path(os.environ.get("DATAKAVACH_BIN", ROOT / "engine" / "build" / "datakavach"))
SETS = {
    "standard": ROOT / "data" / "eval" / "docs.jsonl",  # the phrasing the rules were designed for
    "hard": ROOT / "data" / "eval" / "hard.jsonl",      # written after the rules were frozen
}
OUT = ROOT / "eval" / "results.json"

TYPES = ["AADHAAR", "PAN", "GSTIN", "CARD", "PHONE", "BANK_ACCOUNT", "IFSC", "UPI", "EMAIL", "VOTER_ID", "PASSPORT"]
# Presidio entity -> DataKavach type. Presidio has recognisers for these Indian IDs, but not for
# UPI IDs, IFSC codes or Indian bank account numbers.
PRESIDIO_MAP = {
    "IN_AADHAAR": "AADHAAR", "IN_PAN": "PAN", "IN_GSTIN": "GSTIN", "CREDIT_CARD": "CARD",
    "PHONE_NUMBER": "PHONE", "EMAIL_ADDRESS": "EMAIL", "IN_VOTER": "VOTER_ID", "IN_PASSPORT": "PASSPORT",
}
SHARED = sorted(set(PRESIDIO_MAP.values()), key=TYPES.index)


def load_docs(path=SETS["standard"]):
    with open(path, encoding="utf-8") as f:
        return [json.loads(line) for line in f]


def byte_to_char_map(text: str):
    """byte offset -> char offset, for converting the engine's UTF-8 offsets."""
    m = {}
    b = 0
    for i, ch in enumerate(text):
        m[b] = i
        b += len(ch.encode("utf-8"))
    m[b] = len(text)
    return m


# ------------------------------------------------------------------ predictors

def run_datakavach(docs, threshold=0.5):
    payload = "\0".join(d["text"] for d in docs).encode("utf-8")
    t0 = time.perf_counter()
    res = subprocess.run([str(ENGINE), "detect", "--nul", "--threshold", str(threshold)],
                         input=payload, capture_output=True, check=True)
    elapsed = time.perf_counter() - t0
    lines = res.stdout.decode("utf-8").splitlines()
    assert len(lines) == len(docs), (len(lines), len(docs))
    preds = []
    for d, line in zip(docs, lines):
        m = byte_to_char_map(d["text"])
        preds.append([(m[f["start"]], m[f["end"]], f["type"], f["score"]) for f in json.loads(line)])
    return preds, elapsed


def make_presidio():
    logging.disable(logging.WARNING)  # Presidio logs a lot; tldextract warns when offline
    from presidio_analyzer import AnalyzerEngine
    from presidio_analyzer.nlp_engine import NlpEngineProvider
    from presidio_analyzer.predefined_recognizers import (InAadhaarRecognizer, InGstinRecognizer,
                                                          InPanRecognizer, InPassportRecognizer,
                                                          InVoterRecognizer)
    provider = NlpEngineProvider(nlp_configuration={
        "nlp_engine_name": "spacy", "models": [{"lang_code": "en", "model_name": "en_core_web_sm"}]})
    analyzer = AnalyzerEngine(nlp_engine=provider.create_engine())
    # Presidio ships Indian recognisers but does not enable them by default. Enable all of them,
    # so the baseline is as strong as Presidio can be out of the box.
    for rec in (InAadhaarRecognizer(), InPanRecognizer(), InGstinRecognizer(), InVoterRecognizer(),
                InPassportRecognizer()):
        analyzer.registry.add_recognizer(rec)
    return analyzer


def run_presidio(analyzer, docs):
    entities = list(PRESIDIO_MAP)
    t0 = time.perf_counter()
    raw = [analyzer.analyze(text=d["text"], language="en", entities=entities) for d in docs]
    elapsed = time.perf_counter() - t0
    preds = []
    for results in raw:
        cand = sorted(((r.start, r.end, PRESIDIO_MAP[r.entity_type], r.score) for r in results),
                      key=lambda x: (-x[3], -(x[1] - x[0])))
        kept = []  # like a real user would: keep the best of overlapping results
        for c in cand:
            if all(c[1] <= k[0] or c[0] >= k[1] for k in kept):
                kept.append(c)
        preds.append(kept)
    return preds, elapsed


# ------------------------------------------------------------------ scoring

def iou(a, b):
    inter = max(0, min(a[1], b[1]) - max(a[0], b[0]))
    union = max(a[1], b[1]) - min(a[0], b[0])
    return inter / union if union else 0.0


def score(docs, preds, threshold=0.0, types=TYPES):
    tp, fp, fn = defaultdict(int), defaultdict(int), defaultdict(int)
    by_kind = defaultdict(lambda: [0, 0, 0])
    for d, pred in zip(docs, preds):
        gold = [tuple(s) for s in d["spans"] if s[2] in types]
        pred = [p for p in pred if p[2] in types and p[3] >= threshold]
        matched = set()
        for p in pred:
            hit = next((i for i, g in enumerate(gold)
                        if i not in matched and g[2] == p[2] and iou(p, g) >= 0.5), None)
            if hit is None:
                fp[p[2]] += 1
                by_kind[d["kind"]][1] += 1
            else:
                matched.add(hit)
                tp[p[2]] += 1
                by_kind[d["kind"]][0] += 1
        for i, g in enumerate(gold):
            if i not in matched:
                fn[g[2]] += 1
                by_kind[d["kind"]][2] += 1

    def prf(t, f_p, f_n):
        p = t / (t + f_p) if t + f_p else 0.0
        r = t / (t + f_n) if t + f_n else 0.0
        f1 = 2 * p * r / (p + r) if p + r else 0.0
        return {"tp": t, "fp": f_p, "fn": f_n, "precision": round(p, 4), "recall": round(r, 4), "f1": round(f1, 4)}

    per_type = {t: prf(tp[t], fp[t], fn[t]) for t in types if tp[t] + fp[t] + fn[t]}
    overall = prf(sum(tp.values()), sum(fp.values()), sum(fn.values()))
    kinds = {k: prf(*v) for k, v in sorted(by_kind.items())}
    return {"overall": overall, "per_type": per_type, "by_kind": kinds}


def best_threshold(docs, preds, types):
    """Presidio's scores mean something different from ours; give it its best threshold."""
    best = None
    for th in (0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8):
        s = score(docs, preds, th, types)
        if best is None or s["overall"]["f1"] > best[1]["overall"]["f1"]:
            best = (th, s)
    return best


# ------------------------------------------------------------------ speed

def engine_throughput(docs, repeat_to_mb=40):
    """Scan a corpus made of the eval documents (repeated), single thread and all threads."""
    tmp = ROOT / "data" / "bench"
    corpus = tmp / "eval-corpus"
    corpus.mkdir(parents=True, exist_ok=True)
    blob = ("\n".join(d["text"] for d in docs) + "\n").encode("utf-8")
    reps = max(1, repeat_to_mb * 10**6 // len(blob))
    per_file = max(1, reps // 8)
    for k in range(8):
        (corpus / f"part-{k}.txt").write_bytes(blob * per_file)
    out = {}
    for threads in (1, os.cpu_count() or 1):
        res = subprocess.run([str(ENGINE), "scan", str(corpus), "--threads", str(threads)],
                             capture_output=True, check=True)
        summary = json.loads(res.stdout)["summary"]
        out[f"threads_{threads}"] = {"mb_per_second": round(summary["mb_per_second"], 1),
                                     "megabytes": round(summary["bytes"] / 1e6, 1)}
    return out


def fmt_table(name, s):
    rows = [f"\n{name}", f"  {'type':<13} {'precision':>9} {'recall':>7} {'F1':>6}   (tp/fp/fn)"]
    for t, m in s["per_type"].items():
        rows.append(f"  {t:<13} {m['precision']:>9.3f} {m['recall']:>7.3f} {m['f1']:>6.3f}   ({m['tp']}/{m['fp']}/{m['fn']})")
    o = s["overall"]
    rows.append(f"  {'OVERALL':<13} {o['precision']:>9.3f} {o['recall']:>7.3f} {o['f1']:>6.3f}   ({o['tp']}/{o['fp']}/{o['fn']})")
    return "\n".join(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--presidio", action="store_true", help="also run Microsoft Presidio")
    ap.add_argument("--presidio-docs", type=int, default=0, help="limit Presidio to the first N docs (0 = all)")
    ap.add_argument("--threshold", type=float, default=0.5)
    ap.add_argument("--no-speed", action="store_true")
    args = ap.parse_args()

    if not ENGINE.exists():
        sys.exit(f"Engine not built: {ENGINE}\nRun: cmake -S engine -B engine/build && cmake --build engine/build")
    results = {"machine": {"platform": platform.platform(), "cpus": os.cpu_count()},
               "shared_types": SHARED, "threshold": args.threshold, "sets": {}}
    analyzer = make_presidio() if args.presidio else None

    for set_name, path in SETS.items():
        if not path.exists():
            print(f"(skipping {set_name}: {path.relative_to(ROOT)} not found - run data/generate.py and data/hard.py)")
            continue
        docs = load_docs(path)
        text_mb = sum(len(d["text"].encode()) for d in docs) / 1e6
        entry = {"dataset": {"documents": len(docs), "labelled_values": sum(len(d["spans"]) for d in docs),
                             "megabytes": round(text_mb, 3)}}
        preds, secs = run_datakavach(docs, args.threshold)
        dk_all = score(docs, preds, 0.0, TYPES)
        entry["datakavach"] = {"all_types": dk_all, "shared_types": score(docs, preds, 0.0, SHARED)}
        print(f"\n=== {set_name.upper()} SET: {len(docs)} documents ===")
        print(fmt_table(f"DataKavach (threshold {args.threshold}) - all {len(TYPES)} types", dk_all))

        if analyzer is not None:
            pdocs = docs[: args.presidio_docs] if args.presidio_docs else docs
            run_presidio(analyzer, pdocs[:5])  # warm-up (model load)
            ppreds, psecs = run_presidio(analyzer, pdocs)
            th, p_shared = best_threshold(pdocs, ppreds, SHARED)
            pmb = sum(len(d["text"].encode()) for d in pdocs) / 1e6
            entry["presidio"] = {"threshold": th, "all_types": score(pdocs, ppreds, th, TYPES),
                                 "shared_types": p_shared, "documents": len(pdocs),
                                 "seconds": round(psecs, 3), "mb_per_second": round(pmb / psecs, 3)}
            dk_same = score(pdocs, preds[: len(pdocs)], 0.0, SHARED)
            entry["datakavach"]["shared_types_same_docs"] = dk_same
            print(fmt_table(f"Presidio (its best threshold, {th}) - the {len(SHARED)} types it supports", p_shared))
            print(fmt_table("DataKavach - the same types, same documents", dk_same))
        results["sets"][set_name] = entry

    if not args.no_speed and "standard" in results["sets"]:
        docs = load_docs(SETS["standard"])
        speed = {"datakavach": engine_throughput(docs)}
        print("\nSpeed (DataKavach scan, MB/s):", json.dumps(speed["datakavach"]))
        if "presidio" in results["sets"]["standard"]:
            speed["presidio_mb_per_second"] = results["sets"]["standard"]["presidio"]["mb_per_second"]
            print(f"Speed (Presidio analyze): {speed['presidio_mb_per_second']} MB/s")
        results["speed"] = speed

    results["generated_at"] = time.strftime("%Y-%m-%d %H:%M")
    OUT.write_text(json.dumps(results, indent=2))
    print(f"\nWritten {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
