import { readFile } from "node:fs/promises";
import path from "node:path";
import TypeBadge from "@/components/TypeBadge";
import { typeLabel } from "@/lib/types";

export const metadata = { title: "Benchmarks · DataKavach" };
export const dynamic = "force-dynamic";

const KIND_NOTES = {
  ocr_noise: "OCR mistakes: the letter O instead of a zero, double spaces",
  line_break: "An Aadhaar number broken across two lines",
  odd_phone_formats: "Phone formats like (+91) 98765-43210 or 98765.43210",
  obfuscated_email: "Emails written as “name [at] gmail [dot] com”",
  unknown_contexts: "Business numbers behind words it doesn’t know (“folio”, “consignment”)",
  lower_case_ids: "PAN / GSTIN typed in lower case",
  missing_keywords: "IDs next to words it doesn’t know (“PP no”, rare UPI banks)",
  other_scripts: "Tamil and Devanagari digits",
  realistic_mix: "A realistic email with account, IFSC and PAN",
  invoice_with_gst: "Tax invoices with two GSTINs",
};

async function load() {
  try {
    const file = path.resolve(/*turbopackIgnore: true*/ process.cwd(), "..", "eval", "results.json");
    return JSON.parse(await readFile(file, "utf8"));
  } catch {
    return null;
  }
}

function F1Bars({ dk, pr }) {
  const types = Object.keys(dk.per_type);
  return (
    <div>
      {types.map((t) => (
        <div key={t} style={{ marginBottom: 10 }}>
          <div className="bar-row">
            <span>
              <TypeBadge type={t} />
            </span>
            <div className="bar-track" title={`DataKavach F1 ${dk.per_type[t].f1}`}>
              <i style={{ width: `${dk.per_type[t].f1 * 100}%`, background: "var(--indigo)" }} />
            </div>
            <span className="mono small">{dk.per_type[t].f1.toFixed(2)}</span>
          </div>
          {pr && (
            <div className="bar-row" style={{ marginTop: -2 }}>
              <span />
              <div className="bar-track" style={{ height: 7 }} title={`Presidio F1 ${pr.per_type[t]?.f1 ?? 0}`}>
                <i style={{ width: `${(pr.per_type[t]?.f1 ?? 0) * 100}%`, background: "#a39a8a" }} />
              </div>
              <span className="mono small muted">{(pr.per_type[t]?.f1 ?? 0).toFixed(2)}</span>
            </div>
          )}
        </div>
      ))}
    </div>
  );
}

function Overall({ label, m, strong }) {
  return (
    <tr>
      <td>{strong ? <strong>{label}</strong> : label}</td>
      <td className="num">{m.precision.toFixed(3)}</td>
      <td className="num">{m.recall.toFixed(3)}</td>
      <td className="num">{strong ? <strong>{m.f1.toFixed(3)}</strong> : m.f1.toFixed(3)}</td>
    </tr>
  );
}

export default async function Benchmarks() {
  const r = await load();
  if (!r) {
    return (
      <>
        <div className="page-head">
          <div>
            <span className="kicker">Benchmarks</span>
            <h1>No results yet.</h1>
          </div>
        </div>
        <div className="callout">
          Run <code>python data/generate.py &amp;&amp; python data/hard.py</code>, then{" "}
          <code>python eval/evaluate.py --presidio</code> from the project folder, and reload this page.
        </div>
      </>
    );
  }

  const std = r.sets.standard;
  const hard = r.sets.hard;
  const dkSpeed = r.speed?.datakavach;
  const single = dkSpeed?.threads_1?.mb_per_second;
  const prSpeed = r.speed?.presidio_mb_per_second;
  const ratio = single && prSpeed ? Math.round(single / prSpeed) : null;
  const maxSpeed = Math.max(1, ...Object.values(dkSpeed ?? {}).map((v) => v.mb_per_second));

  return (
    <>
      <div className="page-head">
        <div>
          <span className="kicker">Benchmarks</span>
          <h1>Honest numbers, including where it fails.</h1>
          <p className="lede">
            Measured against Microsoft Presidio, the best-known open-source tool, with all of Presidio’s Indian
            recognisers switched on and its best threshold chosen for it. Two test sets: one written alongside the
            rules, and a harder one written after the rules were frozen.
          </p>
        </div>
        <span className="stamp blue">measured {r.generated_at?.slice(0, 10)}</span>
      </div>

      <div className="tally" style={{ marginBottom: 22 }}>
        <div>
          <span className="label">Hard set · F1, all 11 types</span>
          <div className="big">{hard ? hard.datakavach.all_types.overall.f1.toFixed(2) : "—"}</div>
        </div>
        <div>
          <span className="label">Standard set · F1</span>
          <div className="big">{std.datakavach.all_types.overall.f1.toFixed(3)}</div>
        </div>
        <div>
          <span className="label">Faster than Presidio (1 thread)</span>
          <div className="big">{ratio ? `${new Intl.NumberFormat("en-IN").format(ratio)}×` : "—"}</div>
        </div>
        <div>
          <span className="label">Types Presidio can’t find</span>
          <div className="big">
            3<small>UPI · IFSC · bank a/c</small>
          </div>
        </div>
      </div>

      <div className="grid-2">
        {[
          ["Standard set", std, "Same phrasing the rules were designed around — read this as an upper bound."],
          ["Hard set", hard, "Written after the rules were frozen, never used for tuning. The honest number."],
        ]
          .filter(([, s]) => s)
          .map(([name, s, note]) => (
            <section className="sheet" key={name}>
              <div className="sheet-head">
                <h2>{name}</h2>
                <span className="label">
                  {s.dataset.documents} docs · {s.dataset.labelled_values} values
                </span>
              </div>
              <div className="sheet-body">
                <p className="note">{note}</p>
                <table className="ledger" style={{ marginBottom: 18 }}>
                  <thead>
                    <tr>
                      <th>On the {r.shared_types.length} types both support</th>
                      <th className="num">Precision</th>
                      <th className="num">Recall</th>
                      <th className="num">F1</th>
                    </tr>
                  </thead>
                  <tbody>
                    <Overall label="DataKavach" m={(s.datakavach.shared_types_same_docs ?? s.datakavach.shared_types).overall} strong />
                    {s.presidio && <Overall label={`Presidio (threshold ${s.presidio.threshold})`} m={s.presidio.shared_types.overall} />}
                  </tbody>
                </table>
                <div className="spread" style={{ marginBottom: 8 }}>
                  <span className="label">F1 by type</span>
                  <span className="small">
                    <span style={{ color: "var(--indigo)" }}>▬</span> DataKavach&nbsp;&nbsp;
                    {s.presidio && <><span style={{ color: "#a39a8a" }}>▬</span> Presidio</>}
                  </span>
                </div>
                <F1Bars dk={s.datakavach.shared_types_same_docs ?? s.datakavach.shared_types} pr={s.presidio?.shared_types} />
              </div>
            </section>
          ))}
      </div>

      {hard && (
        <section className="sheet" style={{ marginTop: 22 }}>
          <div className="sheet-head">
            <h2>Where it breaks</h2>
            <span className="stamp">known limits</span>
          </div>
          <div className="sheet-body">
            <table className="ledger">
              <thead>
                <tr>
                  <th>Hard-set case</th>
                  <th className="num">Found</th>
                  <th className="num">Missed</th>
                  <th className="num">False alarms</th>
                  <th className="num">Recall</th>
                </tr>
              </thead>
              <tbody>
                {Object.entries(hard.datakavach.all_types.by_kind)
                  .sort((a, b) => a[1].recall - b[1].recall)
                  .map(([kind, m]) => (
                    <tr key={kind}>
                      <td>{KIND_NOTES[kind] ?? kind}</td>
                      <td className="num">{m.tp}</td>
                      <td className="num">{m.fn}</td>
                      <td className="num">{m.fp}</td>
                      <td className="num">{m.tp + m.fn ? m.recall.toFixed(2) : "—"}</td>
                    </tr>
                  ))}
              </tbody>
            </table>
            <p className="note" style={{ marginTop: 12, marginBottom: 0 }}>
              These are rule-based limits: a checksum can’t read “O” as zero, and a keyword list can’t know every
              business word. Fixing them one by one would only teach the rules this test set — which is why the set is kept frozen.
            </p>
          </div>
        </section>
      )}

      <section className="sheet" style={{ marginTop: 22 }}>
        <div className="sheet-head">
          <h2>Speed</h2>
          <span className="label">{r.machine.cpus} CPU cores · {r.machine.platform.split("-").slice(0, 2).join(" ")}</span>
        </div>
        <div className="sheet-body">
          {dkSpeed &&
            Object.entries(dkSpeed).map(([k, v]) => (
              <div className="bar-row wide" key={k}>
                <span className="small">DataKavach · {k.replace("threads_", "")} thread{k === "threads_1" ? "" : "s"}</span>
                <div className="bar-track">
                  <i style={{ width: `${(v.mb_per_second / maxSpeed) * 100}%`, background: "var(--indigo)", opacity: k === "threads_1" ? 0.7 : 1 }} />
                </div>
                <span className="mono small">{v.mb_per_second} MB/s</span>
              </div>
            ))}
          {prSpeed && (
            <div className="bar-row wide">
              <span className="small">Presidio · 1 thread</span>
              <div className="bar-track">
                <i style={{ width: `${Math.max(0.6, (prSpeed / maxSpeed) * 100)}%`, background: "#a39a8a" }} />
              </div>
              <span className="mono small">{prSpeed} MB/s</span>
            </div>
          )}
          <p className="note" style={{ marginTop: 12, marginBottom: 0 }}>
            The test text is dense on purpose (a value every ~60 bytes); ordinary files have far less personal data
            and scan faster. Presidio runs a spaCy language model on every sentence, which is where most of its time goes.
          </p>
        </div>
      </section>

      <p className="small muted" style={{ marginTop: 18 }}>
        Method: a prediction is correct when its type matches and it overlaps a labelled value by ≥ 50% (IoU).
        All data is synthetic — {typeLabel("AADHAAR")}, PAN, GSTIN and card numbers pass their real checksums but were generated for this project.
        Reproduce with <code>python eval/evaluate.py --presidio</code>.
      </p>
    </>
  );
}
