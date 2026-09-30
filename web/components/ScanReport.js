"use client";
import { useRef, useState } from "react";
import TypeBadge from "@/components/TypeBadge";
import { TYPE_ORDER, TYPES, typeLabel } from "@/lib/types";

function fmt(n) {
  return new Intl.NumberFormat("en-IN").format(n);
}

function sum(obj = {}) {
  return Object.values(obj).reduce((a, b) => a + b, 0);
}

function Heatmap({ report }) {
  const types = TYPE_ORDER.filter((t) => report.summary.by_type[t]);
  const rows = [];
  const files = [...report.files].filter((f) => !f.error).sort((a, b) => b.findings - a.findings);
  for (const f of files) {
    rows.push({ kind: "file", name: f.path, sub: `${fmt(f.lines)} lines`, counts: f.by_type });
    for (const c of f.columns || []) {
      if (sum(c.by_type) > 0) rows.push({ kind: "col", name: `↳ ${c.name}`, counts: c.by_type });
    }
  }
  const max = Math.max(1, ...rows.flatMap((r) => types.map((t) => r.counts[t] || 0)));
  const alpha = (n) => (n ? 0.18 + 0.82 * (Math.log1p(n) / Math.log1p(max)) : 0);

  if (!types.length) return <p className="note">No personal data found in these files.</p>;
  return (
    <div className="heat-wrap">
      <table className="heat">
        <thead>
          <tr>
            <th />
            {types.map((t) => (
              <th key={t} title={TYPES[t].check}>
                {typeLabel(t)}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map((r, i) => (
            <tr key={i} className={r.kind === "col" ? "col-row" : undefined}>
              <th className="rowh" scope="row">
                {r.name}
                {r.sub && <span className="sub">{r.sub}</span>}
              </th>
              {types.map((t) => {
                const n = r.counts[t] || 0;
                return (
                  <td key={t} className={n ? undefined : "zero"} style={{ "--c": TYPES[t].color, "--a": alpha(n) }} title={`${r.name.replace("↳ ", "")}: ${fmt(n)} ${typeLabel(t)}`}>
                    {n ? fmt(n) : "·"}
                  </td>
                );
              })}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function Hotspots({ report }) {
  // The places a DPDP review would look at first: CSV columns and files with the most IDs.
  const spots = [];
  for (const f of report.files) {
    if (f.columns?.length) {
      for (const c of f.columns) if (sum(c.by_type)) spots.push({ where: `${f.path} → column “${c.name}”`, counts: c.by_type });
    } else if (f.findings) {
      spots.push({ where: f.path, counts: f.by_type });
    }
  }
  // Government IDs and financial data weigh more than contact details; log() so that 3,000 phone
  // numbers in a log don't outrank a whole column of Aadhaar numbers.
  const HIGH = new Set(["AADHAAR", "PAN", "CARD", "BANK_ACCOUNT", "PASSPORT", "VOTER_ID"]);
  const weight = (c) => Object.entries(c).reduce((a, [t, n]) => a + Math.log1p(n) * (HIGH.has(t) ? 10 : 1), 0);
  spots.sort((a, b) => weight(b.counts) - weight(a.counts));
  return (
    <ol style={{ margin: 0, paddingLeft: 20 }}>
      {spots.slice(0, 5).map((s) => (
        <li key={s.where} style={{ margin: "0 0 10px" }}>
          <div className="mono small" style={{ marginBottom: 4 }}>{s.where}</div>
          <div className="row" style={{ gap: 5 }}>
            {Object.entries(s.counts)
              .sort((a, b) => b[1] - a[1])
              .map(([t, n]) => (
                <span key={t} className="row" style={{ gap: 4 }}>
                  <TypeBadge type={t} />
                  <span className="mono small">{fmt(n)}</span>
                </span>
              ))}
          </div>
        </li>
      ))}
    </ol>
  );
}

export default function ScanReport() {
  const [report, setReport] = useState(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [over, setOver] = useState(false);
  const input = useRef(null);

  async function run(body) {
    setBusy(true);
    setError("");
    try {
      const r = await fetch("/api/scan", { method: "POST", ...(body ? { body } : {}) });
      const data = await r.json();
      if (!r.ok) throw new Error(data.error || "Scan failed");
      setReport(data);
    } catch (e) {
      setError(e.message);
    } finally {
      setBusy(false);
    }
  }

  function upload(fileList) {
    const files = [...fileList];
    if (!files.length) return;
    const form = new FormData();
    for (const f of files) form.append("files", f, f.webkitRelativePath || f.name);
    run(form);
  }

  function download() {
    const blob = new Blob([JSON.stringify(report, null, 2)], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "datakavach-report.json";
    a.click();
    URL.revokeObjectURL(a.href);
  }

  const s = report?.summary;

  return (
    <div className="stack">
      <div className="grid-2 even">
        <div className="sheet">
          <div className="sheet-head">
            <h3>Try the sample company</h3>
            <span className="label">9 files · 0.6 MB</span>
          </div>
          <div className="sheet-body">
            <p className="small muted">
              A made-up company share: a CRM export, HR sheet, vendor list, payment log, support tickets in English,
              Hinglish and Hindi, server logs — and a product catalogue with no personal data at all.
            </p>
            <button className="btn" onClick={() => run()} disabled={busy}>
              {busy ? "Scanning…" : "Scan sample folder"}
            </button>
          </div>
        </div>
        <div
          className={`dropzone${over ? " over" : ""}`}
          onClick={() => input.current?.click()}
          onDragOver={(e) => {
            e.preventDefault();
            setOver(true);
          }}
          onDragLeave={() => setOver(false)}
          onDrop={(e) => {
            e.preventDefault();
            setOver(false);
            upload(e.dataTransfer.files);
          }}
          role="button"
          tabIndex={0}
          onKeyDown={(e) => (e.key === "Enter" || e.key === " ") && input.current?.click()}
        >
          <input ref={input} type="file" multiple hidden onChange={(e) => upload(e.target.files)} />
          <h3 style={{ marginBottom: 6 }}>…or drop your own files</h3>
          <p className="small muted" style={{ margin: 0 }}>
            CSV, TSV, JSON, logs, text — up to 25 MB. Files are scanned on this machine and deleted straight after.
          </p>
        </div>
      </div>

      {error && <div className="error">{error}</div>}

      {report && (
        <>
          <div className="tally">
            <div>
              <span className="label">Files scanned</span>
              <div className="big">
                {fmt(s.files)}
                {s.skipped > 0 && <small>+{s.skipped} skipped</small>}
              </div>
            </div>
            <div>
              <span className="label">Files with personal data</span>
              <div className="big" style={{ color: s.files_with_pii ? "var(--vermilion)" : "var(--moss)" }}>
                {fmt(s.files_with_pii)}
              </div>
            </div>
            <div>
              <span className="label">Values found</span>
              <div className="big">{fmt(s.findings)}</div>
            </div>
            <div>
              <span className="label">Speed</span>
              <div className="big">
                {s.mb_per_second >= 1 ? Math.round(s.mb_per_second) : s.mb_per_second.toFixed(2)}
                <small>MB/s · {s.threads} threads</small>
              </div>
            </div>
          </div>

          <section className="sheet">
            <div className="sheet-head">
              <h2>Where it lives</h2>
              <button className="btn ghost small" onClick={download}>Download report</button>
            </div>
            <div className="sheet-body">
              <Heatmap report={report} />
              <p className="note" style={{ marginTop: 12, marginBottom: 0 }}>
                Darker ink = more values. CSV files are broken down by column, so you can see that
                “last_order_ref” is clean even though its numbers look like Aadhaar numbers.
              </p>
            </div>
          </section>

          <div className="grid-2 side">
            <section className="sheet">
              <div className="sheet-head">
                <h3>Look here first</h3>
                <span className="stamp">review</span>
              </div>
              <div className="sheet-body">
                <Hotspots report={report} />
              </div>
            </section>
          <section className="sheet">
            <div className="sheet-head">
              <h3>Evidence</h3>
              <span className="label">samples per file · already masked</span>
            </div>
            <div className="sheet-body table-wrap">
              <table className="ledger">
                <thead>
                  <tr>
                    <th>File</th>
                    <th className="num">Line</th>
                    <th>Type</th>
                    <th>Value</th>
                  </tr>
                </thead>
                <tbody>
                  {report.files
                    .filter((f) => f.samples?.length)
                    .flatMap((f) =>
                      f.samples.slice(0, 3).map((smp, i) => (
                        <tr key={`${f.path}-${i}`}>
                          <td className="mono small">{i === 0 ? f.path : ""}</td>
                          <td className="num">{smp.line}</td>
                          <td>
                            <TypeBadge type={smp.type} />
                          </td>
                          <td className="mono val">{smp.value}</td>
                        </tr>
                      )),
                    )}
                </tbody>
              </table>
            </div>
          </section>
          </div>
        </>
      )}
    </div>
  );
}
