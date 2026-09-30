import { mkdtemp, mkdir, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { errorResponse, PROJECT_ROOT, scanPath } from "@/lib/engine";

export const runtime = "nodejs";

const SAMPLE = path.join(/*turbopackIgnore: true*/ PROJECT_ROOT, "data", "sample_company");
const MAX_BYTES = 25 * 1024 * 1024;

// Report paths relative to the scanned folder, so no server paths leak into the page.
function relativize(report, root) {
  for (const f of report.files) f.path = path.relative(root, f.path) || path.basename(f.path);
  return report;
}

export async function POST(req) {
  const type = req.headers.get("content-type") || "";
  try {
    if (type.includes("multipart/form-data")) {
      const form = await req.formData();
      const files = form.getAll("files").filter((f) => typeof f === "object" && f.name);
      if (!files.length) return Response.json({ error: "Choose at least one file" }, { status: 400 });
      const total = files.reduce((n, f) => n + f.size, 0);
      if (total > MAX_BYTES) return Response.json({ error: "Upload is larger than 25 MB" }, { status: 413 });
      const dir = await mkdtemp(path.join(os.tmpdir(), "datakavach-"));
      try {
        for (const f of files) {
          // Keep folder structure from a folder upload, but never allow "../" to escape the temp dir.
          const rel = (f.name || "file.txt").split(/[\\/]+/).filter((p) => p && p !== "." && p !== "..").join(path.sep);
          const dest = path.join(/*turbopackIgnore: true*/ dir, rel || "file.txt");
          await mkdir(path.dirname(dest), { recursive: true });
          await writeFile(dest, Buffer.from(await f.arrayBuffer()));
        }
        const report = await scanPath(dir);
        return Response.json({ source: "upload", ...relativize(report, dir) });
      } finally {
        await rm(dir, { recursive: true, force: true }); // uploaded files are deleted right after the scan
      }
    }
    const report = await scanPath(SAMPLE);
    return Response.json({ source: "sample", ...relativize(report, SAMPLE) });
  } catch (e) {
    return errorResponse(e);
  }
}
