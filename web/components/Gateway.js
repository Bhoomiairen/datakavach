"use client";
import { useEffect, useRef, useState } from "react";

const EXAMPLES = [
  "Draft a short SMS to Bhoomi Airen  (+91 9301947774) saying her KYC with Aadhaar 7260 1815 9082 is approved.",
  "Write a polite email to our vendor, GSTIN 27AAPFU0939F1ZV, asking them to confirm bank a/c no 50100234567891 and IFSC HDFC0049746.",
  "Summarise this complaint in one line: customer rahul.verma@okaxis says the refund to card 4111 1111 1111 1111 never came.",
];

function Tokens({ text }) {
  return text.split(/(\[[A-Z_]+_\d+\])/g).map((p, i) =>
    /^\[[A-Z_]+_\d+\]$/.test(p) ? (
      <span key={i} style={{ color: "var(--vermilion)", fontWeight: 600 }}>{p}</span>
    ) : (
      p
    ),
  );
}

export default function Gateway() {
  const [health, setHealth] = useState(null);
  const [messages, setMessages] = useState([]); // { role, content, raw? }
  const [last, setLast] = useState(null);
  const [input, setInput] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const end = useRef(null);

  useEffect(() => {
    fetch("/api/health").then((r) => r.json()).then(setHealth).catch(() => setHealth(null));
  }, []);
            useEffect(() => {
       end.current?.scrollIntoView({ behavior: "smooth", block: "nearest" });
     }, [messages, busy]);
  async function send(text) {
    const q = text.trim();
    if (!q || busy) return;
    const next = [...messages, { role: "user", content: q }];
    setMessages(next);
    setInput("");
    setBusy(true);
    setError("");
    try {
      const r = await fetch("/api/gateway", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ messages: next }),
      });
      const data = await r.json();
      if (!r.ok) throw new Error(data.error || "Request failed");
      setLast({ original: q, ...data });
      if (data.llm) {
        setMessages([...next, { role: "assistant", content: data.reply, raw: data.raw }]);
      } else {
        setMessages(next.slice(0, -1)); // keep the prompt out of history when no model answered
        setInput(q);
      }
    } catch (e) {
      setError(e.message);
      setMessages(next.slice(0, -1));
      setInput(q);
    } finally {
      setBusy(false);
    }
  }

  const noModel = health && (!health.ollama.reachable || !health.ollama.hasModel);

  return (
    <div className="stack">
      {noModel && (
        <div className="callout warn">
          <strong>No local AI model found</strong> — the masking still works and is shown below, but there is no answer.
          To get answers, start Ollama: <code>ollama serve</code> and <code>ollama pull {health.ollama.model}</code>.
        </div>
      )}

      <section className="sheet">
        <div className="sheet-head">
          <h2>What leaves the building</h2>
          <span className="label">{last ? `${last.tokens.length} value${last.tokens.length === 1 ? "" : "s"} hidden` : "send a message"}</span>
        </div>
        <div className="sheet-body">
          <div className="pipeline">
            <div className="step">
              <span className="label">1 · you typed</span>
              <div className="mono">{last?.original ?? "—"}</div>
            </div>
            <div className="step" style={{ borderColor: "var(--vermilion)" }}>
              <span className="label" style={{ color: "var(--vermilion)" }}>2 · the AI model saw</span>
              <div className="mono">{last ? <Tokens text={last.maskedPrompt} /> : "—"}</div>
            </div>
            <div className="step">
              <span className="label">3 · you got back</span>
              <div className="mono">
                {last ? (last.llm ? last.reply : <span className="muted">{last.llmError}</span>) : "—"}
              </div>
            </div>
          </div>
          {last?.llm && last.raw !== last.reply && (
            <details style={{ marginTop: 10 }}>
              <summary className="small muted" style={{ cursor: "pointer" }}>The model’s raw answer, before the real values were put back</summary>
              <div className="masked-out" style={{ marginTop: 8, minHeight: 0 }}>
                <Tokens text={last.raw} />
              </div>
            </details>
          )}
        </div>
      </section>

      <section className="sheet stacked">
        <div className="sheet-head">
          <h3>Chat</h3>
          <span className="label">{health?.ollama?.reachable ? `model: ${health.ollama.model}` : "model: offline"}</span>
        </div>
        <div className="sheet-body stack" style={{ gap: 14 }}>
          {messages.length === 0 && (
            <div className="chips">
              {EXAMPLES.map((e) => (
                <button key={e} className="chip" style={{ textAlign: "left", lineHeight: 1.35, borderRadius: 10 }} onClick={() => send(e)} disabled={busy}>
                  {e}
                </button>
              ))}
            </div>
          )}
          <div className="chat">
            {messages.map((m, i) => (
              <div key={i} className={`bubble ${m.role === "user" ? "user" : "bot"}`}>
                {m.content}
              </div>
            ))}
            {busy && <div className="note">masking, then asking the model…</div>}
            <div ref={end} />
          </div>
          {error && <div className="error">{error}</div>}
          <form
            className="row"
            style={{ alignItems: "stretch" }}
            onSubmit={(e) => {
              e.preventDefault();
              send(input);
            }}
          >
            <textarea
              className="paper-input"
              style={{ minHeight: 64, flex: 1 }}
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === "Enter" && !e.shiftKey) {
                  e.preventDefault();
                  send(input);
                }
              }}
              placeholder="Ask anything — personal data is swapped for placeholders before the model sees it"
              aria-label="Message"
            />
            <button className="btn" disabled={busy || !input.trim()}>Send</button>
          </form>
        </div>
      </section>
    </div>
  );
}
