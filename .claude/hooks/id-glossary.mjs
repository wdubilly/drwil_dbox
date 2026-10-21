#!/usr/bin/env node
// Glossary of the identifiers, agent side (PostToolUse on the tools that bring text in — shell, reads,
// searches; not the agent's own edits, which would echo what it just wrote): when a tool's output cites
// identifiers the project knows (contract IDs, catalog lines, lots and points of the active brief),
// their labels are slipped into the agent's context, with the rule « always the ID followed by what it
// designates ». The labels come from .githooks/glossary.mjs, the single source, also used by the checks'
// outputs and by any agent through its command. Never blocking: an error leaves the session untouched.
import { mkdirSync, readFileSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const TEXTS = {
  fr: (list) => `[drwil] Identifiants cités ci-dessus : ${list}. À l'humain, toujours l'ID suivi de ce qu'il désigne.`,
  en: (list) => `[drwil] Identifiers cited above: ${list}. To the human, always the ID followed by what it designates.`,
};

// Identifiers already recalled in this session (local, gitignored like the state): repeating the
// same labels after every tool made the reminder noise nobody read.
const SEEN = [".drwil", "glossary.json"];

/** The identifiers of `found` not recalled yet in `session`; records them. Unreadable memory: all are new. */
function onlyNew(found, session, root) {
  const id = typeof session === "string" ? session : "";
  let seen = [];
  try {
    const memory = JSON.parse(readFileSync(join(root, ...SEEN), "utf8"));
    if (memory?.session === id && Array.isArray(memory.ids)) seen = memory.ids;
  } catch {}
  const fresh = found.filter((f) => !seen.includes(f.id));
  if (fresh.length) {
    try {
      mkdirSync(join(root, ".drwil"), { recursive: true });
      writeFileSync(join(root, ...SEEN), `${JSON.stringify({ session: id, ids: [...seen, ...fresh.map((f) => f.id)] })}\n`);
    } catch {}
  }
  return fresh;
}

/** The context to inject for this tool output, or null when it cites nothing known. */
export async function contextFor(input, root) {
  const { cited, glossary } = await import(pathToFileURL(join(root, ".githooks", "glossary.mjs")).href);
  const output = typeof input?.tool_response === "string" ? input.tool_response : JSON.stringify(input?.tool_response ?? "");
  const found = onlyNew(cited(output, glossary(root)), input?.session_id, root);
  if (!found.length) return null;
  let lang = "fr";
  try {
    lang = JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr";
  } catch {}
  return TEXTS[lang](found.map((f) => `${f.id} — ${f.label}`).join(" ; "));
}

// Real paths on both sides: a project path through a symbolic link must still run the hook.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  try {
    // The folder holding this hook's .claude/, never the current folder (an agent's `cd`).
    const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
    const text = await contextFor(JSON.parse(readFileSync(0, "utf8")), root);
    if (text) console.log(JSON.stringify({ hookSpecificOutput: { hookEventName: "PostToolUse", additionalContext: text } }));
  } catch {}
}
