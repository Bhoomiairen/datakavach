// Server-side bridge to the C++ engine. Every request starts the binary once and talks to it
// through stdin/stdout (the engine starts in a few milliseconds, so no daemon is needed).
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";

export const ENGINE = process.env.DATAKAVACH_BIN || path.resolve(/*turbopackIgnore: true*/ process.cwd(), "..", "engine", "build", "datakavach");
export const PROJECT_ROOT = path.resolve(/*turbopackIgnore: true*/ process.cwd(), "..");

export function engineAvailable() {
  return existsSync(ENGINE);
}

export class EngineError extends Error {
  constructor(message, status = 500) {
    super(message);
    this.status = status;
  }
}

function run(args, input = "", timeoutMs = 60_000) {
  if (!engineAvailable()) {
    throw new EngineError(
      `The DataKavach engine is not built yet (looked for ${ENGINE}). Build it with: cmake -S engine -B engine/build && cmake --build engine/build`,
      503,
    );
  }
  return new Promise((resolve, reject) => {
    const child = spawn(/*turbopackIgnore: true*/ ENGINE, args, { stdio: ["pipe", "pipe", "pipe"] });
    const out = [];
    const err = [];
    const timer = setTimeout(() => {
      child.kill("SIGKILL");
      reject(new EngineError("The engine took too long", 504));
    }, timeoutMs);
    child.stdout.on("data", (d) => out.push(d));
    child.stderr.on("data", (d) => err.push(d));
    child.on("error", (e) => {
      clearTimeout(timer);
      reject(new EngineError(`Could not start the engine: ${e.message}`));
    });
    child.on("close", (code) => {
      clearTimeout(timer);
      if (code !== 0) return reject(new EngineError(Buffer.concat(err).toString() || `engine exited with ${code}`));
      resolve({ stdout: Buffer.concat(out), stderr: Buffer.concat(err).toString() });
    });
    child.stdin.end(input);
  });
}

// The engine reports UTF-8 byte offsets; the browser works in UTF-16 string indices.
function byteToCharIndex(text) {
  const map = new Map();
  let byte = 0;
  let i = 0;
  for (const ch of text) {
    map.set(byte, i);
    byte += Buffer.byteLength(ch, "utf8");
    i += ch.length;
  }
  map.set(byte, i);
  return map;
}

export async function maskText(text, { mode = "partial", threshold = 0.5 } = {}) {
  const { stdout } = await run(["mask", "-", "--json", "--mode", mode, "--threshold", String(threshold)], text);
  const result = JSON.parse(stdout.toString("utf8"));
  const map = byteToCharIndex(text);
  result.findings = result.findings.map((f) => ({ ...f, start: map.get(f.start), end: map.get(f.end) }));
  return result; // { text, findings, vault }
}

export async function scanPath(target, { threads = 0, threshold = 0.5 } = {}) {
  const args = ["scan", target, "--threshold", String(threshold)];
  if (threads) args.push("--threads", String(threads));
  const { stdout } = await run(args, "", 300_000);
  return JSON.parse(stdout.toString("utf8"));
}

export function errorResponse(e) {
  const status = e instanceof EngineError ? e.status : 500;
  return Response.json({ error: e.message || "Something went wrong" }, { status });
}
