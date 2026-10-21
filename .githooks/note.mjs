#!/usr/bin/env node
// Note an idea at any moment, from any tool: `node .githooks/note.mjs "<text>"` appends one dated line
// to the notes file of the intents (`docs/intentions/notes.md`, or `dirs.intentions` of the settings).
// It changes no activity, touches neither the state nor the active brief, commits nothing: a note is an
// intent, never an order to code (QUA-061); it is committed with the branch in progress, outside the index.
import { existsSync, mkdirSync, readFileSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { config } from "./briefs.mjs";
import { stateRoot } from "./state.mjs";

const TEXTS = {
  fr: {
    usage: 'usage : node .githooks/note.mjs "<texte de la note>"',
    empty: "note vide : rien n'est écrit",
    done: (f) => `note ajoutée à ${f} (ni activité changée, ni fiche touchée, rien commité : elle part avec la branche du chantier)`,
    file: (date) => `# Intention : notes en vrac

**Statut** : ouverte en permanence — dernière note le ${date}.

## Besoin

Consigner une idée ou une intention d'une ligne à n'importe quel moment, y compris en plein chantier, sans la
cadrer ni la coder : \`node .githooks/note.mjs "<texte>"\`. Une note est toujours une intention, jamais un ordre
de coder. Quand elle mûrit, elle est promue en fiche d'intention et sa ligne est retirée d'ici.

## Existant

Relevé le ${date} : les notes sont ci-dessous, une ligne datée chacune, hors de l'index des chantiers.

## Questions à trancher

- [décision] Promouvoir en intention les notes qui ont mûri, et retirer les autres.

## Notes

`,
  },
  en: {
    usage: 'usage: node .githooks/note.mjs "<text of the note>"',
    empty: "empty note: nothing written",
    done: (f) => `note added to ${f} (no activity changed, no brief touched, nothing committed: it goes with the branch in progress)`,
    file: (date) => `# Intent: loose notes

**Status**: open permanently — last note on ${date}.

## Need

Record a one-line idea or intent at any moment, even in the middle of a workstream, without framing or coding
it: \`node .githooks/note.mjs "<text>"\`. A note is always an intent, never an order to code. When it matures,
it is promoted to an intent brief and its line is removed from here.

## Current state

Read on ${date}: the notes are below, one dated line each, outside the workstream index.

## Open questions

- [decision] Promote the notes that matured to intents, and remove the others.

## Notes

`,
  },
};

const STATUS = /^(\*\*(?:Statut|Status)\*\*[^\n]*?)\d{4}-\d{2}-\d{2}/m;

/** The notes file path, from the root. */
export const notesPath = (root) => `${config(root).dirs?.intentions ?? "docs/intentions"}/notes.md`;

/**
 * Appends `- <date> — <text>` to the notes file (created if absent) and dates its Status line, so that
 * the file is never reported stale while notes arrive. `{ code, message }`.
 */
export function note(root, text, { now = new Date() } = {}) {
  const T = TEXTS[config(root).lang === "en" ? "en" : "fr"];
  const line = String(text).replace(/\s+/g, " ").trim();
  if (!line) return { code: 2, message: T.empty };
  const date = now.toISOString().slice(0, 10);
  const rel = notesPath(root);
  const file = join(root, rel);
  const body = existsSync(file) ? readFileSync(file, "utf8") : T.file(date);
  const dated = body.replace(STATUS, `$1${date}`);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, `${dated.endsWith("\n") ? dated : `${dated}\n`}- ${date} — ${line}\n`);
  return { code: 0, message: T.done(rel) };
}

/** Entry point: `node .githooks/note.mjs "<text>"`. Returns the exit code. */
export function main(args, root, opts) {
  const T = TEXTS[config(root).lang === "en" ? "en" : "fr"];
  if (args.some((a) => a.startsWith("-"))) {
    console.error(T.usage);
    return 2;
  }
  const r = note(root, args.join(" "), opts);
  (r.code === 0 ? console.log : console.error)(r.code === 2 && !args.length ? T.usage : r.message);
  return r.code;
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = main(process.argv.slice(2), stateRoot());
}
