// QUA-140, QUA-141: fingerprints of what the checks judge, split from engine.mjs (which re-exports them) to keep it under the size cap.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { globToRegex } from "./glob.mjs";

/**
 * QUA-140: fingerprint of what a check judges (commit, branch, uncommitted changes incl. untracked files not ignored, local state,
 * configuration; not the evidence files of the pass itself): one byte of difference changes it. `null` if git does not answer: nothing is then reusable.
 */
export function treeFingerprint(root) {
  try {
    const git = (...args) => spawnSync("git", args, { cwd: root, maxBuffer: 1 << 30 });
    const h = createHash("sha256");
    const part = (b) => h.update(`${Buffer.byteLength(b)}:`).update(b);
    const outs = [
      "rev-parse HEAD",
      "symbolic-ref -q HEAD",
      "diff HEAD --binary --no-ext-diff",
      "ls-files -o --exclude-standard -z -- :(exclude,glob).drwil/evidence/*",
    ].map((a) => git(...a.split(" ")));
    if (outs.some((r, i) => r.status !== 0 && i !== 1)) return null;
    for (const r of outs) part(r.stdout);
    const read = (f) => (existsSync(join(root, f)) ? readFileSync(join(root, f)) : "");
    for (const f of outs[3].stdout.toString().split("\0").filter(Boolean)) part(read(f));
    for (const f of [".drwil/ia-first.json", ".drwil/state.json"]) part(read(f));
    return h.digest("hex");
  } catch {
    return null;
  }
}

/**
 * QUA-141: fingerprint of what ONE check judges — its declaration, this engine and the content of the files its `paths` match
 * (tracked, and untracked not ignored; not the evidence of the pass): one byte of difference changes it. A Map id → fingerprint of the
 * `list` entries that declare `paths` (and are not `reuse: false` or `reuse: "tree"`); `null` for every other one, for a check whose
 * `paths` match no file and for one a file of which is unreadable: nothing is then reusable for it. A check that reads a file its `paths`
 * omit would be reused wrongly: the project declares it `reuse: "tree"` (fingerprint of the whole tree only) or completes its `paths`.
 */
export function controlFingerprints(root, list) {
  const out = new Map(list.map((c) => [c.id, null]));
  const wanted = list.filter((c) => c.perControl);
  if (!wanted.length) return out;
  const r = spawnSync("git", ["ls-files", "-c", "-o", "--exclude-standard", "-z"], { cwd: root, maxBuffer: 1 << 30 });
  if (r.status !== 0) return out;
  const files = r.stdout
    .toString()
    .split("\0")
    .filter((f) => f && !f.startsWith(".drwil/evidence/"))
    .sort();
  const engine = createHash("sha256")
    .update(readFileSync(fileURLToPath(import.meta.url)))
    .update(readFileSync(fileURLToPath(new URL("./engine.mjs", import.meta.url))))
    .digest("hex");
  const hashes = new Map();
  const hashOf = (f) => {
    if (!hashes.has(f)) {
      try {
        hashes.set(
          f,
          createHash("sha256")
            .update(readFileSync(join(root, f)))
            .digest("hex"),
        );
      } catch (e) {
        hashes.set(f, e.code === "ENOENT" ? "-" : null);
      }
    }
    return hashes.get(f);
  };
  for (const c of wanted) {
    const patterns = c.paths.map(globToRegex);
    const h = createHash("sha256").update(`${engine}\0${c.declaration}\0`);
    let judged = 0;
    for (const f of files) {
      if (!patterns.some((p) => p.test(f))) continue;
      const hash = hashOf(f);
      if (hash === null) {
        judged = 0;
        break;
      }
      judged++;
      h.update(`${f}\0${hash}\n`);
    }
    if (judged) out.set(c.id, h.digest("hex"));
  }
  return out;
}
