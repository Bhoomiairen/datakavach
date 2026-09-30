import Playground from "@/components/Playground";

const STEPS = [
  ["Normalise", "Digits in nine Indian scripts (०-९, ௦-௯, …) become 0-9, with a map back to the original text."],
  ["Tokenise", "Runs of letters and digits; groups like “9876 5432 1012” are tried in every combination."],
  ["Validate", "Checksums decide: Verhoeff for Aadhaar, mod-36 for GSTIN, Luhn for cards, structure rules for PAN, IFSC…"],
  ["Context", "One Aho-Corasick pass finds 110+ keywords — “aadhaar”, “mob no”, “आधार”, and warning words like “order id”."],
  ["Resolve", "The best non-overlapping findings above the strictness line are kept, then masked."],
];

export default function Home() {
  return (
    <>
      <div className="page-head">
        <div>
          <span className="kicker">Playground</span>
          <h1>Find the Aadhaar before it leaks.</h1>
          <p className="lede">
            DataKavach reads text the way an auditor would — checking every number that looks like an Indian ID,
            and only marking the ones that really are. Try the examples or paste your own text; nothing leaves this machine.
          </p>
        </div>
      </div>

      <Playground />

      <section style={{ marginTop: 44 }}>
        <div className="spread" style={{ marginBottom: 14 }}>
          <h2>How one line of text is checked</h2>
          <span className="note">all five steps are linear in the length of the text</span>
        </div>
        <ol className="sheet steps">
          {STEPS.map(([title, body], i) => (
            <li key={title}>
              <span className="step-no">{i + 1}</span>
              <strong>{title}</strong>
              <span className="muted">{body}</span>
            </li>
          ))}
        </ol>
      </section>
    </>
  );
}
