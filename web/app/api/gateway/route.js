import { errorResponse, maskText } from "@/lib/engine";

export const runtime = "nodejs";

const OLLAMA = process.env.OLLAMA_URL || "http://localhost:11434";
const MODEL = process.env.OLLAMA_MODEL || "llama3.2:3b";
const SEPARATOR = "\n␞␞␞\n"; // a line no user types, used to mask the whole chat at once

const SYSTEM = `You are a helpful assistant inside a company. Some details in the user's messages were
replaced by placeholders such as [PHONE_1], [AADHAAR_1], [PAN_1] or [EMAIL_2] before they reached you.
Treat each placeholder as the real value. When you need to mention one, write the placeholder exactly
as given, including the square brackets. Never guess or invent the hidden values.`;

function unmask(text, vault) {
  return text.replace(/\[[A-Z_]+_\d+\]/g, (tok) => vault[tok] ?? tok);
}

// Body: { messages: [{ role: "user" | "assistant", content, raw? }] }
// For assistant turns, `raw` is what the model actually wrote (with placeholders).
export async function POST(req) {
  try {
    const { messages = [], threshold = 0.5 } = await req.json();
    const users = messages.filter((m) => m.role === "user").map((m) => String(m.content || ""));
    if (!users.length || !users.at(-1).trim()) return Response.json({ error: "Type a message" }, { status: 400 });
    if (users.join("").length > 20_000) return Response.json({ error: "Conversation is too long" }, { status: 413 });

    // Mask all user turns in ONE engine call, so the same value keeps the same token across turns.
    const masked = await maskText(users.join(SEPARATOR), { mode: "token", threshold });
    const maskedUsers = masked.text.split(SEPARATOR.trim()).map((s) => s.replace(/^\n|\n$/g, ""));
    if (maskedUsers.length !== users.length) throw new Error("Could not split the masked conversation");

    let u = 0;
    const modelMessages = [{ role: "system", content: SYSTEM }];
    for (const m of messages) {
      if (m.role === "user") modelMessages.push({ role: "user", content: maskedUsers[u++] });
      else modelMessages.push({ role: "assistant", content: String(m.raw ?? m.content ?? "") });
    }

    const lastMasked = maskedUsers.at(-1);
    const tokensInLast = [...new Set(lastMasked.match(/\[[A-Z_]+_\d+\]/g) || [])];
    const base = { maskedPrompt: lastMasked, tokens: tokensInLast, findings: masked.findings.length, model: MODEL };

    let reply;
    try {
      const r = await fetch(`${OLLAMA}/api/chat`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ model: MODEL, messages: modelMessages, stream: false, options: { temperature: 0.2 } }),
        signal: AbortSignal.timeout(120_000),
      });
      if (!r.ok) throw new Error(`Ollama answered ${r.status}${r.status === 404 ? ` (is the model pulled? ollama pull ${MODEL})` : ""}`);
      reply = (await r.json()).message?.content ?? "";
    } catch (e) {
      return Response.json({ ...base, llm: false, llmError: e.message?.includes("fetch failed") ? `Ollama is not running at ${OLLAMA}` : e.message });
    }
    return Response.json({ ...base, llm: true, raw: reply, reply: unmask(reply, masked.vault) });
  } catch (e) {
    return errorResponse(e);
  }
}
