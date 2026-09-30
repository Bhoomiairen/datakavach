import { errorResponse, maskText } from "@/lib/engine";

export const runtime = "nodejs";

const MODES = new Set(["partial", "redact", "token"]);

export async function POST(req) {
  try {
    const body = await req.json();
    const text = typeof body.text === "string" ? body.text : "";
    if (text.length > 200_000) return Response.json({ error: "Text is too long (max 200,000 characters)" }, { status: 413 });
    const mode = MODES.has(body.mode) ? body.mode : "partial";
    const threshold = Math.min(0.95, Math.max(0.05, Number(body.threshold) || 0.5));
    const t0 = performance.now();
    const result = await maskText(text, { mode, threshold });
    return Response.json({ ...result, ms: Math.round(performance.now() - t0) });
  } catch (e) {
    return errorResponse(e);
  }
}
