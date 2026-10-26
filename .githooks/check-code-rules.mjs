#!/usr/bin/env node
// Extension point for machine-checked code hygiene rules (pre-commit hook and
// CI). Empty by default: this check stays green as long as the project adds
// nothing. A project that adopts it adds it to .drwil/ia-first.json -> checks:
//   { "name": "code hygiene rules", "run": "node .githooks/check-code-rules.mjs" }
//
// Each rule is a function (root) => string[] of errors. Examples (disabled, to
// adapt or remove):
//   - no import of a private name (`_name`) from another module of the same package;
//   - a pure-logic module has its test file next to it;
//   - a CSV export goes through a single point (anti-injection escaping);
//   - an isolated subsystem never imports a package from another one.
//
// One rule is built in and always run, by check-docs (QUA-049): a code file cites the contract it
// serves, never a workstream brief — the closing deletes the brief, the citation would go dead.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, realpathSync } from "node:fs";
import { join, posix } from "node:path";
import { fileURLToPath } from "node:url";

const rules = [
  // (root) => { const errors = []; /* ... */ return errors; },
];

const TEXTS = {
  fr: {
    title: "Règles d'hygiène du code :",
    briefRef: (place, brief) =>
      `${place} : cite la fiche de chantier ${brief}, supprimée à la clôture — citer le contrat que ce code sert (son ID) (QUA-049)`,
    legacy: (n, files) =>
      `${n} citation(s) de fiche de chantier dans du code non modifié (non bloquant ; à corriger quand le fichier change, QUA-049) : ${files}`,
  },
  en: {
    title: "Code hygiene rules:",
    briefRef: (place, brief) =>
      `${place}: cites the workstream brief ${brief}, deleted at closing — cite the contract this code serves (its ID) (QUA-049)`,
    legacy: (n, files) =>
      `${n} workstream brief citation(s) in unchanged code (non-blocking; fix them when the file changes, QUA-049): ${files}`,
  },
};

/** The project's language, read from the current folder (the project root, where hooks run). */
export function language() {
  try {
    return JSON.parse(readFileSync(join(".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr";
  } catch {
    return "fr";
  }
}

/** Applies `list` (the rules above by default) to `root`; prints the errors, returns the exit code. */
export function check(root, list = rules, lang = "fr") {
  const errors = existsSync(root) ? list.flatMap((rule) => rule(root)) : [];
  if (!errors.length) return 0;
  console.error(`${TEXTS[lang].title}\n  ${errors.join("\n  ")}`);
  return 1;
}

const git = (root, args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout : null;
};

/** Files changed in the working tree (staged, not staged, untracked), `/` form; outside git: none. */
export function changedFiles(root) {
  const out = git(root, ["status", "--porcelain", "-z", "--untracked-files=all"]);
  const files = [];
  const tokens = (out ?? "").split("\0");
  for (let i = 0; i < tokens.length; i++) {
    if (!tokens[i]) continue;
    files.push(tokens[i].slice(3));
    // A rename or a copy is followed by its source path.
    if (/^[RC]/.test(tokens[i])) i++;
  }
  return files;
}

const CODE_EXT = /\.(mjs|cjs|js|jsx|ts|tsx|py|sh|ya?ml)$/;
// Only comments: a test fixture (a brief path in a string) is data, not a citation.
const COMMENT = /^\s*(\/\/|#|\/\*|\*|<!--)|\s\/\/\s/;
// Files of the briefs folder that are not workstreams: they stay, citing them is fine.
const PERMANENT =
  /^(pending|journal|readme|routine-maintenance|entretien-courant|kit-mechanics|mecanique-ia-first|model-.*|modele-.*)\.md$/i;

/**
 * Citations of a workstream brief in the comments of code files (tracked or new):
 * `[{ file, line, brief, changed }]`, `changed` when the file is changed in the working tree.
 */
export function briefReferences(root, cfg = {}) {
  const dir = (cfg.dirs?.projects ?? "docs/projects").replace(/\/$/, "");
  const index = posix.basename(cfg.dirs?.index ?? "pending.md");
  const cite = new RegExp(`${dir.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}/[\\w.-]+\\.md`, "g");
  const changed = new Set(changedFiles(root));
  const out = git(root, ["ls-files", "-z", "--cached", "--others", "--exclude-standard"]) ?? "";
  const refs = [];
  for (const file of new Set(out.split("\0"))) {
    if (!CODE_EXT.test(file) || file.startsWith("docs/") || !existsSync(join(root, file))) continue;
    readFileSync(join(root, file), "utf8")
      .split(/\r?\n/)
      .forEach((text, i) => {
        const comment = COMMENT.exec(text);
        for (const [brief] of comment ? text.slice(comment.index).matchAll(cite) : []) {
          const name = posix.basename(brief);
          if (!PERMANENT.test(name) && name !== index) refs.push({ file, line: i + 1, brief, changed: changed.has(file) });
        }
      });
  }
  return refs;
}

/**
 * The built-in rule, for check-docs: a citation in a changed file is an error when the project
 * enforces recent requirements (`newRequirements: "blocking"`), a warning otherwise; citations in
 * unchanged files are summed up in one warning — an equipped project is never broken by its past.
 */
export function briefReferenceProblems(root, cfg = {}, lang = "fr") {
  const T = TEXTS[lang === "en" ? "en" : "fr"];
  const errors = [];
  const warnings = [];
  const legacy = [];
  for (const r of briefReferences(root, cfg)) {
    if (!r.changed) legacy.push(r);
    else (cfg.newRequirements === "blocking" ? errors : warnings).push(T.briefRef(`${r.file}:${r.line}`, r.brief));
  }
  if (legacy.length) warnings.push(T.legacy(legacy.length, [...new Set(legacy.map((r) => r.file))].join(", ")));
  return { errors, warnings };
}

// Run from the command line (not imported by a test): real paths compared, otherwise a
// symlinked .githooks/ would make the check pass without verifying anything.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = check(process.argv[2] ?? ".", rules, language());
}
