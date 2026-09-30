import { ENGINE, engineAvailable } from "@/lib/engine";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const OLLAMA = process.env.OLLAMA_URL || "http://localhost:11434";
const MODEL = process.env.OLLAMA_MODEL || "llama3.2:3b";

export async function GET() {
  let ollama = { reachable: false, model: MODEL, hasModel: false };
  try {
    const r = await fetch(`${OLLAMA}/api/tags`, { signal: AbortSignal.timeout(1500) });
    if (r.ok) {
      const { models = [] } = await r.json();
      ollama = { reachable: true, model: MODEL, hasModel: models.some((m) => m.name === MODEL || m.model === MODEL) };
    }
  } catch {
    /* Ollama not running: the gateway still shows the masking step */
  }
  return Response.json({ engine: engineAvailable(), enginePath: engineAvailable() ? undefined : ENGINE, ollama });
}
