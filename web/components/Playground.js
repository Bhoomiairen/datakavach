"use client";
import { useEffect, useRef, useState } from "react";
import TypeBadge, { Score } from "@/components/TypeBadge";
import { SAMPLES } from "@/lib/samples";
import { TYPES, typeLabel, why } from "@/lib/types";

const MODES = [
  { id: "partial", label: "Partial", hint: "XXXX XXXX 9082 — keeps the format and last 4" },
  { id: "redact", label: "Redact", hint: "[AADHAAR] — removes the value completely" },
  { id: "token", label: "Token", hint: "[AADHAAR_1] — reversible, used by the AI gateway" },
];

function Highlighted({ text, findings }) {
  const parts = [];
  let pos = 0;
  findings.forEach((f, i) => {
    if (f.start > pos) parts.push(text.slice(pos, f.start));
    parts.push(
      <mark
        key={i}
        className="pii"
        style={{ "--c": TYPES[f.type]?.color }}
        title={`${typeLabel(f.type)} · confidence ${f.score.toFixed(2)}\n${why(f)}`}
      >
        {text.slice(f.start, f.end)}
      </mark>,
    );
    pos = f.end;
  });
  parts.push(text.slice(pos));
  return <div className="rendered">{parts}</div>;
}

function MaskedText({ text, mode }) {
  if (mode === "partial") return <div className="masked-out">{text}</div>;
  const pieces = text.split(/(\[[A-Z_]+(?:_\d+)?\])/g);
  return (
    <div className="masked-out">
      {pieces.map((p, i) => (/^\[[A-Z_]+(?:_\d+)?\]$/.test(p) ? <span key={i} className="tok">{p}</span> : p))}
    </div>
  );
}

export default function Playground() {
  const [sample, setSample] = useState(SAMPLES[0].id);
  const [text, setText] = useState(SAMPLES[0].text);
  const [mode, setMode] = useState("partial");
  const [threshold, setThreshold] = useState(0.5);
  const [result, setResult] = useState(null);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const [copied, setCopied] = useState(false);
  const seq = useRef(0);

  useEffect(() => {
    const id = ++seq.current;
    const timer = setTimeout(async () => {
      setBusy(true);
      try {
        const r = await fetch("/api/detect", {
          method: "POST",
          headers: { "content-type": "application/json" },
          body: JSON.stringify({ text, mode, threshold }),
        });
        const data = await r.json();
        if (id !== seq.current) return; // a newer request is on its way
        if (!r.ok) throw new Error(data.error || "Request failed");
        setResult({ ...data, source: text });
        setError("");
      } catch (e) {
        if (id === seq.current) setError(e.message);
      } finally {
        if (id === seq.current) setBusy(false);
      }
    }, 220);
    return () => clearTimeout(timer);
  }, [text, mode, threshold]);

  const findings = result && result.source === text ? result.findings : [];
  function pick(s) {
    setSample(s.id);
    setText(s.text);
  }

  async function copy() {
    if (!result) return;
    await navigator.clipboard.writeText(result.text);
    setCopied(true);
    setTimeout(() => setCopied(false), 1400);
  }

  const isHindi = /[ऀ-ॿ]/.test(text);

  return (
    <div className="stack">
      <div className="spread">
        <div className="chips" role="group" aria-label="Examples">
          {SAMPLES.map((s) => (
            <button key={s.id} className="chip" aria-pressed={sample === s.id} onClick={() => pick(s)}>
              {s.label}
            </button>
          ))}
        </div>
        <label className="slider">
          Strictness
          <input type="range" min="0.3" max="0.9" step="0.05" value={threshold} onChange={(e) => setThreshold(Number(e.target.value))} />
          <span className="mono">{threshold.toFixed(2)}</span>
        </label>
      </div>

      <div className="grid-2">
        <section className="sheet stacked">
          <div className="sheet-head">
            <h2>Paste anything</h2>
            <span className="label">{busy ? "reading…" : result ? `${result.ms} ms` : ""}</span>
          </div>
          <div className="sheet-body stack" style={{ gap: 16 }}>
            <textarea
              className="paper-input"
              value={text}
              spellCheck={false}
              onChange={(e) => {
                setText(e.target.value);
                setSample("");
              }}
              aria-label="Text to check"
            />
            <div>
              <div className="spread" style={{ marginBottom: 6 }}>
                <span className="label">What DataKavach sees</span>
                <span className="note">hover a mark to see why</span>
              </div>
              <div lang={isHindi ? "hi" : undefined}>
                <Highlighted text={text} findings={findings} />
              </div>
            </div>
          </div>
        </section>

        <section className="stack">
          <div className="sheet">
            <div className="sheet-head">
              <h2>Safe to share</h2>
              {findings.length > 0 ? <span className="stamp">masked</span> : <span className="stamp green">clean</span>}
            </div>
            <div className="sheet-body stack" style={{ gap: 12 }}>
              <div className="spread">
                <div className="segmented" role="group" aria-label="Masking mode">
                  {MODES.map((m) => (
                    <button key={m.id} aria-pressed={mode === m.id} onClick={() => setMode(m.id)} title={m.hint}>
                      {m.label}
                    </button>
                  ))}
                </div>
                <button className="btn ghost small" onClick={copy} disabled={!result}>
                  {copied ? "Copied" : "Copy"}
                </button>
              </div>
              <p className="small muted" style={{ margin: 0 }}>{MODES.find((m) => m.id === mode).hint}</p>
              {error ? <div className="error">{error}</div> : <MaskedText text={result?.text ?? ""} mode={mode} />}
            </div>
          </div>

          <div className="sheet">
            <div className="sheet-head">
              <h3>Findings</h3>
              <span className="label">{findings.length} found</span>
            </div>
            <div className="sheet-body table-wrap">
              {findings.length === 0 ? (
                <p className="note" style={{ margin: 0 }}>Nothing that looks like Indian personal data at this strictness.</p>
              ) : (
                <table className="ledger">
                  <thead>
                    <tr>
                      <th>Type</th>
                      <th>Value</th>
                      <th className="why">Why</th>
                      <th className="num">Confidence</th>
                    </tr>
                  </thead>
                  <tbody>
                    {findings.map((f, i) => (
                      <tr key={i}>
                        <td>
                          <TypeBadge type={f.type} />
                        </td>
                        <td className="mono val">{text.slice(f.start, f.end)}</td>
                        <td className="small muted why">{why(f)}</td>
                        <td className="num">
                          <Score value={f.score} />
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>
          </div>
        </section>
      </div>
    </div>
  );
}
