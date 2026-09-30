import Gateway from "@/components/Gateway";

export const metadata = { title: "AI gateway · DataKavach" };

export default function GatewayPage() {
  return (
    <>
      <div className="page-head">
        <div>
          <span className="kicker">AI gateway</span>
          <h1>Use AI without handing it your customers.</h1>
          <p className="lede">
            Every message is masked before it reaches the model: Aadhaar, phone numbers and account numbers become
            placeholders like <code>[AADHAAR_1]</code>. The model writes its answer with the placeholders, and DataKavach
            puts the real values back only on your side.
          </p>
        </div>
      </div>
      <Gateway />
    </>
  );
}
