#!/usr/bin/env node
// Guard against giant files (see docs/recipes/refactor-without-breaking.md).
// Extension point to adopt: a project that wants this check adds it to
// .drwil/ia-first.json -> checks, for example:
//   { "name": "file size", "run": "node .githooks/check-file-size.mjs frontend/src" }
// Root, ceiling and extensions as arguments (no dependency on a stack).
// Files already too big before adoption are capped at their current size in
// check-file-size.legacy.json (next to it): they can only shrink; when a
// refactor reduces one, lower or remove its cap.
import { readdirSync, readFileSync, existsSync, realpathSync } from "node:fs";
import { join, relative, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const TEXTS = {
  fr: {
    legacy: "{0} : {1} lignes, plafond hérité {2} (ne doit plus grossir, extraire plutôt)",
    max: "{0} : {1} lignes, maximum {2} (découper)",
    title: "Fichiers trop gros :",
  },
  en: {
    legacy: "{0}: {1} lines, legacy cap {2} (must not grow any more, extract instead)",
    max: "{0}: {1} lines, maximum {2} (split it)",
    title: "Files too big:",
  },
};

/** Fills the `{0}`, `{1}`… holes of a message. */
const fill = (message, ...values) => message.replace(/\{(\d)\}/g, (_, i) => values[i]);

function readJson(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return {};
  }
}

/** Legacy caps ({ file: lines }); missing or unreadable file: none. */
export const readLegacy = readJson;

const ignoreRe = /\.(test|spec)\.[^.]+$/;

function* sources(dir, extRe) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name === "node_modules" || entry.name === "__pycache__" || entry.name.startsWith(".")) continue;
    const path = join(dir, entry.name);
    if (entry.isDirectory()) yield* sources(path, extRe);
    else if (extRe.test(entry.name) && !ignoreRe.test(entry.name)) yield path;
  }
}

/** Files too big under `root` (paths relative to the current folder), one message per file. */
export function check({ root, maxLines, extensions, legacy, lang }) {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const extRe = new RegExp(`\\.(${extensions.join("|")})$`);
  const errors = [];
  if (!existsSync(root)) return errors;
  for (const path of sources(root, extRe)) {
    const file = relative(".", path).replace(/\\/g, "/");
    const lines = readFileSync(path, "utf8").split("\n").length - 1;
    const cap = legacy[file] ?? maxLines;
    if (lines > cap) errors.push(fill(legacy[file] ? T.legacy : T.max, file, lines, legacy[file] ? cap : maxLines));
  }
  return errors;
}

// Run from the command line (not imported by a test): real paths compared, otherwise a
// symlinked .githooks/ would make the check pass without verifying anything.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  const lang = readJson(join(".drwil", "ia-first.json")).lang;
  const errors = check({
    root: process.argv[2] ?? ".",
    maxLines: Number(process.argv[3] ?? 400),
    extensions: (process.argv[4] ?? "ts,tsx,js,jsx,py").split(","),
    legacy: readLegacy(join(dirname(fileURLToPath(import.meta.url)), "check-file-size.legacy.json")),
    lang,
  });
  if (errors.length) {
    console.error(`${TEXTS[lang === "en" ? "en" : "fr"].title}\n  ${errors.join("\n  ")}`);
    process.exitCode = 1;
  }
}
