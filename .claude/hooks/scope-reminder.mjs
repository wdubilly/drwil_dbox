#!/usr/bin/env node
// Agent-side scope for Claude Code (Edit, Write, MultiEdit; docs/ia-first.md section 7,
// docs/projects/kit-mechanics.md), according to .drwil/ia-first.json -> scopeReminder:
// - "blocking" (default): PreToolUse hook, REFUSES writing a code file that no brief of
//   docs/projects/ covers, before it happens: the agent must first attach the file to its
//   workstream (or to docs/projects/routine-maintenance.md).
//   A rule that only holds through the agent's memory is not a guarantee; the commit refuses
//   anyway.
// - "warning": PostToolUse hook, a simple reminder after the write.
// - "off": nothing.
// Any unexpected input, or even an exception, gives "nothing" (the git hook stays the gate).
// The grammar comes from .githooks/scope.mjs, the same as the commit-time check.
import { existsSync, readFileSync, readdirSync, realpathSync } from "node:fs";
import { basename, dirname, join, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

// The folder holding this hook's .claude/, never the current folder: after an agent's `cd`, the
// current folder has no .githooks/ and the guard would refuse nothing.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
// Gemini CLI: write_file and replace, events BeforeTool and AfterTool.
const TOOLS = new Set(["Edit", "Write", "MultiEdit", "write_file", "replace"]);
const MODEL_RE = /^modele-|^model-/;

const TEXTS = {
  fr: {
    refused: (rel) =>
      `Écriture refusée (QUA-016, scopeReminder : blocking) : aucune fiche de docs/projects/ ne couvre ${rel}. ` +
      "Rattacher d'abord ce chemin, ou un motif qui le couvre, au bloc « scope » de la fiche du chantier " +
      "en cours (ou à docs/projects/routine-maintenance.md pour une petite tâche), puis réessayer.",
    reminder: (rel) =>
      `Rappel de cadrage : aucune fiche de docs/projects/ ne couvre ${rel}. Ajouter ce chemin, ou un ` +
      "motif qui le couvre, au bloc « scope » de la fiche du chantier en cours (ou à " +
      "docs/projects/routine-maintenance.md pour une petite tâche). " +
      'Rien n\'est bloqué ici, ni au commit (réglage "scopeReminder" de .drwil/ia-first.json : warning).',
  },
  en: {
    refused: (rel) =>
      `Write refused (QUA-016, scopeReminder: blocking): no brief of docs/projects/ covers ${rel}. ` +
      'First attach this path, or a pattern that covers it, to the "scope" block of the current ' +
      "workstream's brief (or to docs/projects/routine-maintenance.md for a small task), then try again.",
    reminder: (rel) =>
      `Scope reminder: no brief of docs/projects/ covers ${rel}. Add this path, or a pattern that ` +
      'covers it, to the "scope" block of the current workstream\'s brief (or to ' +
      "docs/projects/routine-maintenance.md for a small task). " +
      'Nothing is blocked here, nor at commit time ("scopeReminder" setting of .drwil/ia-first.json: warning).',
  },
};

function loadConfig(root) {
  try {
    return JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8"));
  } catch {
    return {};
  }
}

function mdRecursive(dir) {
  if (!existsSync(dir)) return [];
  return readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
    const p = join(dir, e.name);
    return e.isDirectory() ? mdRecursive(p) : e.name.endsWith(".md") ? [p] : [];
  });
}

/** Resolves symlinks up to the first existing ancestor (the file targeted by a Write does not
 * exist yet), to compare two paths on the same real base: macOS resolves /tmp and /var to
 * /private/... on the first `cd`, which `resolve()` alone does not see. */
function realpathAncestor(path) {
  let p = resolve(path);
  const rest = [];
  while (!existsSync(p)) {
    rest.unshift(basename(p));
    const parent = dirname(p);
    if (parent === p) return resolve(path);
    p = parent;
  }
  return rest.length ? join(realpathSync(p), ...rest) : realpathSync(p);
}

function relativePath(path, root) {
  try {
    const r = relative(realpathAncestor(root), realpathAncestor(path));
    return r.startsWith("..") ? null : r.replace(/\\/g, "/");
  } catch {
    return null;
  }
}

async function remind(input, root = ROOT) {
  let event;
  try {
    event = JSON.parse(input);
  } catch {
    return "";
  }
  if (typeof event !== "object" || event === null || !TOOLS.has(event.tool_name)) return "";
  // Claude Code et Gemini CLI envoient file_path ; Copilot CLI envoie path pour les mêmes outils.
  const path = event.tool_input?.file_path ?? event.tool_input?.path;
  if (typeof path !== "string" || !path) return "";
  const rel = relativePath(path, root);
  if (!rel) return "";

  const scope = await import(pathToFileURL(join(root, ".githooks", "scope.mjs")).href);
  const cfg = loadConfig(root);
  if (cfg.scopeReminder === "off") return "";
  if (!scope.isCode(rel, cfg)) return "";

  const projectsDir = join(root, cfg.dirs?.projects ?? "docs/projects");
  const briefs = mdRecursive(projectsDir).filter((p) => !MODEL_RE.test(relative(projectsDir, p)));
  const briefTexts = briefs.map((f) => [relative(root, f), readFileSync(f, "utf8")]);
  const byBrief = scope.patternsByBrief(briefTexts, cfg);
  if (scope.briefsCovering(rel, byBrief).length) return "";

  const T = TEXTS[cfg.lang === "en" ? "en" : "fr"];
  const blocking = (cfg.scopeReminder ?? "blocking") === "blocking";
  // Without an event name (direct call, older configuration): PostToolUse behaviour.
  const before = event.hook_event_name === "PreToolUse" || event.hook_event_name === "BeforeTool";
  if (before && blocking) {
    // Gemini CLI reads a top-level decision, not Claude Code's permissionDecision.
    if (event.hook_event_name === "BeforeTool") return JSON.stringify({ decision: "deny", reason: T.refused(rel) });
    return JSON.stringify({
      hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: "deny", permissionDecisionReason: T.refused(rel) },
    });
  }
  if (before || blocking) return "";
  const after = event.hook_event_name === "AfterTool" ? "AfterTool" : "PostToolUse";
  return JSON.stringify({ hookSpecificOutput: { hookEventName: after, additionalContext: T.reminder(rel) } });
}

/** Entry point of the hook: never an error, at worst no reminder. */
async function main(input) {
  try {
    return await remind(input);
  } catch {
    return "";
  }
}

export { remind, main };

// Real paths on both sides: a project path through a symbolic link must still run the guard.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  let input = "";
  process.stdin.on("data", (d) => {
    input += d;
  });
  process.stdin.on("end", async () => {
    const out = await main(input);
    if (out) console.log(out);
  });
}
