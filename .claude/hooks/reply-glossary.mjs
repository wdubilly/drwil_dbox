#!/usr/bin/env node
// Glossary at the end of the agent's reply (Stop): the identifiers the reply cites bare — no gloss
// right after them (QUA-039) — are shown to the human under the reply, « ID — label ». The reply is
// already displayed: this completes it, never rewrites it, never blocks, and goes through
// `systemMessage` (shown to the human), not `additionalContext` (which the model would read and pay
// for). Labels from .githooks/glossary.mjs, the single source. Any error: nothing.
import { readFileSync, realpathSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const TEXTS = {
  fr: (list) => `[drwil] Identifiants cités sans libellé : ${list}`,
  en: (list) => `[drwil] Identifiers cited without a label: ${list}`,
};

/** The message to show for this reply, or null when every known identifier is labelled. */
export async function noteFor(input, root) {
  const reply = input?.last_assistant_message;
  if (typeof reply !== "string" || !reply) return null;
  const { glossary, unlabelled } = await import(pathToFileURL(join(root, ".githooks", "glossary.mjs")).href);
  const bare = unlabelled(reply, glossary(root));
  if (!bare.length) return null;
  let lang = "fr";
  try {
    lang = JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr";
  } catch {}
  return TEXTS[lang](bare.map((b) => `${b.id} — ${b.label}`).join(" ; "));
}

// Real paths on both sides: a project path through a symbolic link must still run the hook.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  try {
    // The folder holding this hook's .claude/, never the current folder (QUA-044).
    const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
    const note = await noteFor(JSON.parse(readFileSync(0, "utf8")), root);
    if (note) console.log(JSON.stringify({ systemMessage: note }));
  } catch {}
}
