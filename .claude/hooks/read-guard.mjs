#!/usr/bin/env node
// Targeted reading, enforced on Claude Code's side (PreToolUse hook, Read matcher; QUA-132).
//
// A file read whole stays in the agent's context and is read again at every request: the contract
// registry and the journal weigh tens of thousands of tokens each. Reading either of them without a
// range (neither offset nor limit) is refused before it happens, with the targeted readings of
// .githooks/glossary.mjs instead. A read by range (needed to edit) and a search (Grep) stay allowed.
// Never blocks wrongly: an unexpected input, a file absent from the project or an internal error
// lets the read through.
import { existsSync, readFileSync, realpathSync } from "node:fs";
import { dirname, isAbsolute, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

// The folder holding this hook's .claude/, never the current folder (an agent's `cd`, QUA-044).
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

const TEXTS = {
  fr: (rel) =>
    `Lecture refusée (QUA-132, lecture ciblée) : ${rel} lu en entier resterait dans le contexte et serait relu à chaque requête. ` +
    "Un contrat : `node .githooks/glossary.mjs --contract <ID>` ; la liste de tous les contrats : " +
    "`node .githooks/glossary.mjs --contracts` ; l'entrée de journal d'un chantier : " +
    "`node .githooks/glossary.mjs --journal <chantier>`. Pour éditer, lire par plage (offset et limit) ; une recherche (Grep) reste permise.",
  en: (rel) =>
    `Read refused (QUA-132, targeted reading): ${rel} read whole would stay in the context and be read again at every request. ` +
    "One contract: `node .githooks/glossary.mjs --contract <ID>`; the list of every contract: " +
    "`node .githooks/glossary.mjs --contracts`; a workstream's journal entry: " +
    "`node .githooks/glossary.mjs --journal <workstream>`. To edit, read by range (offset and limit); a search (Grep) stays allowed.",
};

function config(root) {
  try {
    return JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8"));
  } catch {
    return {};
  }
}

/** The guarded files, relative to the root with `/`: the same paths .githooks/glossary.mjs reads. */
export function guardedFiles(root = ROOT, cfg = config(root)) {
  const norm = (p) => p.replace(/\\/g, "/").replace(/^\.\//, "");
  return [norm(cfg.dirs?.contracts ?? "docs/contracts.md"), norm(`${cfg.dirs?.projects ?? "docs/projects"}/journal.md`)];
}

/** The refusal message, in the project's language. */
export function refusal(rel, root = ROOT, cfg = config(root)) {
  return TEXTS[cfg.lang === "en" ? "en" : "fr"](rel);
}

const real = (p) => (existsSync(p) ? realpathSync(p) : null);

/** The guarded file (relative path) that `path` designates in the project, or null. */
export function guardedTarget(path, root = ROOT, cfg = config(root)) {
  if (typeof path !== "string" || !path) return null;
  const asked = real(isAbsolute(path) ? path : resolve(root, path));
  if (!asked) return null;
  return guardedFiles(root, cfg).find((rel) => real(join(root, rel)) === asked) ?? null;
}

/** JSON answer of the hook, or an empty string to let the read through. */
export function decide(input, root = ROOT) {
  const event = JSON.parse(input);
  if (typeof event !== "object" || event === null || event.tool_name !== "Read") return "";
  const args = event.tool_input ?? {};
  if (args.offset != null || args.limit != null) return "";
  const cfg = config(root);
  const rel = guardedTarget(args.file_path, root, cfg);
  if (!rel) return "";
  return JSON.stringify({
    hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: "deny", permissionDecisionReason: refusal(rel, root, cfg) },
  });
}

/** Entry point of the hook: never an error, at worst the read goes through. */
export function main(input, root = ROOT) {
  try {
    return decide(input, root);
  } catch {
    return "";
  }
}

// Real paths on both sides: a project path through a symbolic link must still run the guard.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  let input = "";
  process.stdin.on("data", (d) => {
    input += d;
  });
  process.stdin.on("end", () => {
    const out = main(input);
    if (out) console.log(out);
  });
}
