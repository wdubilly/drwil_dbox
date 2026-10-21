#!/usr/bin/env node
// IA-first check: paths and contracts cited in the docs.
// Reads .drwil/ia-first.json to adapt to the layers, the contract prefixes and the language.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, relative, sep } from "node:path";
import { execFileSync } from "node:child_process";
import * as scope from "./scope.mjs";
import { definedIds, idConflicts, knownChecks, readContracts, validateContracts } from "./contracts.mjs";
import { checkRisk } from "./risk.mjs";
import { briefReferenceProblems } from "./check-code-rules.mjs";
import { briefMaturity, briefStatus, readyGaps } from "./state.mjs";
import { skillHeaderProblems } from "./skill-headers.mjs";

const root = process.cwd();
const cfg = loadConfig();
const lang = cfg.lang === "en" ? "en" : "fr";
const T = {
  fr: {
    path: (c) => `chemin cité introuvable \`${c}\``,
    contract: (id, f) => `contrat ${id} absent de ${f}`,
    duplicate: (id, f) => `${f} : contrat ${id} défini plusieurs fois`,
    marker: () => `case ouverte sans marqueur [IA], [humain] ou [décision]`,
    status: (f) => `${f} : pas de ligne « Statut » datée (AAAA-MM-JJ) en tête`,
    handOff: (f) => `${f} : section « Reprise » absente (passation)`,
    section: (f, s) => `${f} : section « ${s} » absente`,
    outsideBriefs: (c) => `\`${c}\` : fichier de code hors de toute fiche de docs/projects/ (bloc cadrage)`,
    scopeHeader: (f) => `${f} : bloc scope sans ligne « files: »`,
    scopeLine: (f, l) => `${f} : ligne de cadrage non comprise « ${l} »`,
    scopeBroad: (f, p) => `${f} : motif de cadrage trop large « ${p} »`,
    summary: (n, e) => `check-docs : ${n} fichiers scannés, ${e} erreur(s)`,
    scopeWarnings: (n) =>
      `check-docs : ${n} avertissement(s) de cadrage (QUA-016, non bloquant — réglage "scopeReminder" de .drwil/ia-first.json)`,
    openButDone: (brief) => `case ouverte mais la fiche ${brief} se dit déjà terminée (QUA-015)`,
    checkedButOpen: (brief) => `case cochée mais la fiche ${brief} ne se dit pas terminée (QUA-015)`,
    briefDone: (brief) =>
      `${brief} : fiche terminée encore présente — la condenser dans docs/projects/journal.md, la supprimer et retirer sa ligne de l'index (QUA-015)`,
    consistencyWarnings: (n) => `check-docs : ${n} avertissement(s) de cohérence case/statut (QUA-015, non bloquant)`,
    readyUndecided: (f) => `${f} : fiche « prête » mais un point [décision] reste à trancher (section « Points à trancher »)`,
    readyNoIncrement: (f) => `${f} : fiche « prête » mais sans aucun lot (« - **Lot … »)`,
    readyNoCriteria: (f) => `${f} : fiche « prête » mais sans critère d'acceptation (section « Critères d'acceptation »)`,
    riskWarnings: (n) => `check-docs : ${n} avertissement(s) de niveau de risque (non bloquant)`,
    ignoredByGit: "fichier ignoré par Git : absent en CI (ajouter « (si présent) »)",
    newWarnings: (n) =>
      `check-docs : ${n} avertissement(s) d'exigence récente (réglage « newRequirements » : warning ; « blocking » pour l'imposer)`,
  },
  en: {
    path: (c) => `cited path not found \`${c}\``,
    contract: (id, f) => `contract ${id} missing from ${f}`,
    duplicate: (id, f) => `${f}: contract ${id} defined more than once`,
    marker: () => `open item without an [AI], [human] or [decision] marker`,
    status: (f) => `${f}: no dated « Status » line (YYYY-MM-DD) at the top`,
    handOff: (f) => `${f}: missing « Hand-off » section`,
    section: (f, s) => `${f}: missing « ${s} » section`,
    outsideBriefs: (c) => `\`${c}\`: code file outside any docs/projects/ brief (scope block)`,
    scopeHeader: (f) => `${f}: scope block without a « files: » line`,
    scopeLine: (f, l) => `${f}: scope line not understood « ${l} »`,
    scopeBroad: (f, p) => `${f}: scope pattern too broad « ${p} »`,
    summary: (n, e) => `check-docs: ${n} files scanned, ${e} error(s)`,
    scopeWarnings: (n) => `check-docs: ${n} scope warning(s) (QUA-016, non-blocking — "scopeReminder" setting in .drwil/ia-first.json)`,
    openButDone: (brief) => `item unchecked but ${brief} already says it is done (QUA-015)`,
    checkedButOpen: (brief) => `item checked but ${brief} does not say it is done (QUA-015)`,
    briefDone: (brief) =>
      `${brief}: finished card still present — condense it into docs/projects/journal.md, delete it and remove its index line (QUA-015)`,
    consistencyWarnings: (n) => `check-docs: ${n} checkbox/status consistency warning(s) (QUA-015, non-blocking)`,
    readyUndecided: (f) => `${f}: brief marked "ready" but a [decision] point is still open (« Points to decide » section)`,
    readyNoIncrement: (f) => `${f}: brief marked "ready" but has no increment ("- **Increment …")`,
    readyNoCriteria: (f) => `${f}: brief marked "ready" but has no acceptance criterion (« Acceptance criteria » section)`,
    riskWarnings: (n) => `check-docs: ${n} risk level warning(s) (non-blocking)`,
    ignoredByGit: 'file ignored by Git: missing in CI (add "(if present)")',
    newWarnings: (n) => `check-docs: ${n} recent-requirement warning(s) (setting "newRequirements": warning; "blocking" to enforce it)`,
  },
}[lang];

const BRIEF_CITATION_RE = /`((?:docs\/projects|docs\/intentions)\/[^`]+\.md)`/g;

// Severity of the scope reminder (QUA-016 only; the rest of check-docs always stays blocking):
// "blocking" (historical default), "warning" (never blocking, only shown) or "off" (disabled).
const SCOPE_LEVELS = new Set(["blocking", "warning", "off"]);
const scopeLevel = SCOPE_LEVELS.has(cfg.scopeReminder) ? cfg.scopeReminder : "warning";

const CODE_RE = /`([^`\s]+)`/g;
const EXT_RE = /\.(md|mdc|mjs|cjs|js|ts|tsx|jsx|py|sh|json|ya?ml|toml|txt)$/;
// A line that declares the file optional must not fail the check.
const OPTIONAL_RE = /si présent|if present/i;
// A planned path not created yet, marked right after the citation, is not an error.
const TO_CREATE_RE = /^\s*\(à créer\)|^\s*\(to create\)/i;
const MARKERS_RE = /\[(IA|humain|décision|AI|human|decision)\]/i;
// tolerates a date in parentheses right after the word (e.g. "**Status** (2026-10-04):").
const STATUS_RE = /^\*{0,2}(Statut|Status)\*{0,2}\s?(\([^)]*\))?\s?:/;
const DATE_RE = /\b\d{4}-\d{2}-\d{2}\b/;
// Intent sections, in either language (an intent written before a language switch stays valid), named in the project's.
const INTENT_SECTIONS = [
  ["## Besoin", "## Need"],
  ["## Existant", "## Current state"],
  ["## Questions à trancher", "## Open questions"],
];
// Models to copy and permanent brief: not workstreams with increments, exempt from the QUA-015 check.
const EXEMPT_RE = /^modele-|^model-|^entretien-courant\.md$|^routine-maintenance\.md$|^journal\.md$/;
// Models to copy: never a real source of scope (their block, if any, is only an example).
const MODEL_RE = /^modele-|^model-/;

function loadConfig() {
  try {
    return JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8"));
  } catch {
    return {};
  }
}

function mdRecursive(dir) {
  if (!existsSync(dir)) return [];
  return readdirSync(dir, { withFileTypes: true })
    .flatMap((e) => {
      const p = join(dir, e.name);
      if (e.isDirectory()) return mdRecursive(p);
      return e.name.endsWith(".md") ? [p] : [];
    })
    .sort();
}

function sources() {
  const found = ["AGENTS.md", "CLAUDE.md", "GEMINI.md", "README.md", "INSTALL.md"].map((f) => join(root, f));
  for (const layer of cfg.layers ?? []) found.push(join(root, layer, "AGENTS.md"));
  found.push(...mdRecursive(join(root, "docs")));
  found.push(...mdRecursive(join(root, ".claude", "skills")));
  return [...new Set(found)].filter((p) => existsSync(p));
}

// Known prefixes (root + project layers): a citation outside them without a known extension is no path ("minimal/full").
const ROOT_PREFIXES = [
  "docs/",
  ".githooks/",
  ".claude/",
  ".github/",
  ".cursor/",
  ".drwil/",
  "scripts/",
  "AGENTS.md",
  "README.md",
  "INSTALL.md",
  "CLAUDE.md",
  "GEMINI.md",
  ".gitlab-ci.yml",
  ".gitignore",
  ".env.example",
  "docker-compose.yml",
];
const knownPrefixes = [...ROOT_PREFIXES, ...(cfg.layers ?? []).map((l) => `${l}/`), ...(cfg.layerPrefixes ?? []).map((p) => `${p}/`)];

function looksLikeAPath(s) {
  if (/^(https?:\/\/|-|\$)/.test(s) || /[<>{}*?|=:@]/.test(s)) return false;
  if (EXT_RE.test(s)) return true;
  return knownPrefixes.some((p) => s.startsWith(p));
}

// A path can be relative to the repository root or to the file that cites it.
const foundPaths = (doc, cited) => [join(root, cited), join(dirname(doc), cited)].filter((p) => existsSync(p));

// "/" form: Git, even on Windows, does not recognise as ignored a path written with backslashes (seen in Windows CI).
const relPosix = (p) => relative(root, p).split(sep).join("/");

/**
 * Among these paths, those Git ignores (a single call). Present on the machine but missing in CI,
 * they do not prove a citation: without this, the local check passed and CI failed. By default,
 * `check-ignore` does not report a tracked file. Outside a Git repository: none.
 */
function ignoredByGit(paths) {
  const rels = [...new Set(paths.map(relPosix))].filter((r) => r && !r.startsWith(".."));
  if (!rels.length) return new Set();
  try {
    const out = execFileSync("git", ["check-ignore", "--stdin"], {
      cwd: root,
      input: rels.join("\n"),
      stdio: ["pipe", "pipe", "ignore"],
    });
    return new Set(out.toString().split(/\r?\n/).filter(Boolean));
  } catch {
    return new Set(); // exit code 1: nothing ignored; or no git repository.
  }
}

const prefixes = cfg.contractPrefixes ?? ["SEC", "QUA"];
// "other-repo:QUA-011" cites another repository's contract: it is not looked up in this registry.
const idPattern = `(?<![\\w:-])(?:${prefixes.map((p) => p.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|")})-\\d{3}\\b`;
const contractsPath = join(root, cfg.dirs?.contracts ?? "docs/contracts.md");
let defined = new Set();
const errors = [];
if (existsSync(contractsPath)) {
  const registry = definedIds(readFileSync(contractsPath, "utf8"), idPattern);
  defined = registry.defined;
  for (const id of registry.duplicates) errors.push(T.duplicate(id, relative(root, contractsPath)));
  // A malformed contract (unknown check, missing Rule or proof) is refused from the commit on,
  // with the same reader as `drwil verify`; duplicates are already reported above.
  for (const e of validateContracts(readContracts(readFileSync(contractsPath, "utf8")), knownChecks(cfg), lang)) {
    if (e.type !== "duplicate") errors.push(`${relative(root, contractsPath)}:${e.line} : ${e.message}`);
  }
}

const docs = sources();
const citations = [];
for (const doc of docs) {
  const rel = relative(root, doc);
  const docLines = readFileSync(doc, "utf8").split(/\r?\n/);
  docLines.forEach((line, i) => {
    // "(if present)" may fall on the next line after a line break.
    if (!OPTIONAL_RE.test(line) && !OPTIONAL_RE.test(docLines[i + 1] ?? "")) {
      for (const m of line.matchAll(CODE_RE)) {
        const cited = m[1];
        if (looksLikeAPath(cited) && !TO_CREATE_RE.test(line.slice(m.index + m[0].length))) {
          citations.push({ place: `${rel}:${i + 1}`, cited, found: foundPaths(doc, cited) });
        }
      }
    }
    if (doc !== contractsPath) {
      for (const [id] of line.matchAll(new RegExp(idPattern, "g"))) {
        if (!defined.has(id)) errors.push(`${rel}:${i + 1} : ${T.contract(id, relative(root, contractsPath))}`);
      }
    }
  });
}

const ignored = ignoredByGit(citations.flatMap((c) => c.found));
// Requirement added after projects were already equipped: an ignored file no longer proves a citation. Blocking only
// if the project has it (`newRequirements`: "blocking", default of fresh installs); otherwise reported, never breaking.
const newBlocking = cfg.newRequirements === "blocking";
const newWarnings = [];
const catalogPath = join(root, cfg.dirs?.catalog ?? "docs/contracts-catalog.md");
for (const p of existsSync(catalogPath) ? idConflicts(readFileSync(catalogPath, "utf8"), defined, relative(root, contractsPath), lang) : [])
  (newBlocking ? errors : newWarnings).push(`${relative(root, catalogPath)} : ${p}`);
for (const c of citations) {
  if (!c.found.length) errors.push(`${c.place} : ${T.path(c.cited)}`);
  else if (!c.found.some((p) => !ignored.has(relPosix(p))))
    (newBlocking ? errors : newWarnings).push(`${c.place} : ${T.path(c.cited)} — ${T.ignoredByGit}`);
}

errors.push(...checkWorkstreams(), ...skillHeaderProblems(root, lang));
// QUA-049: a code file cites the contract it serves, never a workstream brief (deleted at closing).
const codeRefs = briefReferenceProblems(root, cfg, lang);
errors.push(...codeRefs.errors);

// Risk level of project briefs (ceremony follows risk); QUA-016 traceability does not change.
const riskWarnings = [];
{
  const projectsDir = join(root, cfg.dirs?.projects ?? "docs/projects");
  const indexPath = join(root, cfg.dirs?.index ?? "docs/projects/pending.md");
  // The kit mechanics brief is shipped with every install and describes no workstream: warning
  // about it from the install of every project would be noise, not a signal.
  const mechanics = /^mecanique-ia-first\.md$|^kit-mechanics\.md$/;
  for (const brief of mdRecursive(projectsDir).filter(
    (p) => p !== indexPath && !EXEMPT_RE.test(relative(projectsDir, p)) && !mechanics.test(relative(projectsDir, p)),
  )) {
    const r = checkRisk(readFileSync(brief, "utf8"), {
      idPattern,
      lang,
      ...(cfg.risk?.sensitivePaths ? { sensitive: cfg.risk.sensitivePaths } : {}),
    });
    for (const e of r.errors) errors.push(`${relative(root, brief)} : ${e}`);
    for (const w of r.warnings) riskWarnings.push(`${relative(root, brief)} : ${w}`);
  }
}

const scopeProblems = scopeLevel === "off" ? [] : checkScopeReminder();
if (scopeLevel === "blocking") errors.push(...scopeProblems);

for (const e of errors) console.log(`  ✗ ${e}`);
console.log(T.summary(docs.length, errors.length));
if (scopeLevel === "warning" && scopeProblems.length) {
  for (const e of scopeProblems) console.log(`  ⚠ ${e}`);
  console.log(T.scopeWarnings(scopeProblems.length));
}
const consistencyProblems = checkCheckboxStatus();
if (consistencyProblems.length) {
  for (const e of consistencyProblems) console.log(`  ⚠ ${e}`);
  console.log(T.consistencyWarnings(consistencyProblems.length));
}
if (riskWarnings.length) {
  for (const e of riskWarnings) console.log(`  ⚠ ${e}`);
  console.log(T.riskWarnings(riskWarnings.length));
}
for (const w of codeRefs.warnings) console.log(`  ⚠ ${w}`);
if (newWarnings.length) {
  for (const e of newWarnings) console.log(`  ⚠ ${e}`);
  console.log(T.newWarnings(newWarnings.length));
}
process.exit(errors.length ? 1 : 0);

// QUA-015 (extension): the box ([ ]/[x]) of an index line must reflect the actual status of the
// brief it cites — a non-blocking warning (same family as QUA-016), because the "Status" text
// stays free (prose), not a structured field that can be checked for sure.
function checkCheckboxStatus() {
  const warnings = [];
  const indexPath = join(root, cfg.dirs?.index ?? "docs/projects/pending.md");
  if (!existsSync(indexPath)) return warnings;
  const relIndex = relative(root, indexPath);
  const lines = readFileSync(indexPath, "utf8").split(/\r?\n/);
  // groups each bullet (an entry may spread over several indented lines): a new bullet starts at "- [ ]" or "- [x]".
  let bullet = null;
  const bullets = [];
  for (let i = 0; i < lines.length; i++) {
    const m = /^\s*-\s*\[( |x|X)\]/.exec(lines[i]);
    if (m) {
      bullet = { checked: m[1].toLowerCase() === "x", line: i + 1, text: lines[i] };
      bullets.push(bullet);
    } else if (bullet && lines[i].trim() !== "" && /^\s+\S/.test(lines[i])) {
      bullet.text += "\n" + lines[i];
    } else {
      bullet = null;
    }
  }
  for (const b of bullets) {
    for (const m of b.text.matchAll(BRIEF_CITATION_RE)) {
      const briefPath = join(root, m[1]);
      if (!existsSync(briefPath)) continue; // reported elsewhere (cited path not found).
      const briefText = readFileSync(briefPath, "utf8");
      if (
        !briefText
          .split(/\r?\n/)
          .slice(0, 15)
          .some((l) => STATUS_RE.test(l))
      )
        continue; // reported elsewhere (no Status line).
      const done = briefStatus(briefText) === "done";
      if (!b.checked && done) warnings.push(`${relIndex}:${b.line} : ${T.openButDone(m[1])}`);
      else if (b.checked && !done) warnings.push(`${relIndex}:${b.line} : ${T.checkedButOpen(m[1])}`);
    }
  }
  return warnings;
}

// QUA-015: each workstream can be resumed cold (markers, dated status, sections, Hand-off).
function checkWorkstreams() {
  const errors = [];
  const indexPath = join(root, cfg.dirs?.index ?? "docs/projects/pending.md");
  if (existsSync(indexPath)) {
    const relIndex = relative(root, indexPath);
    readFileSync(indexPath, "utf8")
      .split(/\r?\n/)
      .forEach((line, i) => {
        if (/^\s*-\s*\[ \]/.test(line) && !MARKERS_RE.test(line)) errors.push(`${relIndex}:${i + 1} : ${T.marker()}`);
      });
  }
  const projectsDir = join(root, cfg.dirs?.projects ?? "docs/projects");
  const briefs = mdRecursive(projectsDir).filter((p) => p !== indexPath && !EXEMPT_RE.test(relative(projectsDir, p)));
  for (const brief of briefs) checkBrief(brief, errors, true);
  const intentsDir = join(root, cfg.dirs?.intentions ?? "docs/intentions");
  const intents = mdRecursive(intentsDir).filter((p) => relative(intentsDir, p) !== "README.md");
  for (const intent of intents) checkBrief(intent, errors, false);
  return errors;
}

// Scope reminder: an indexed code file must be covered by the `scope` block of a brief of
// docs/projects/ (including the permanent "routine maintenance" brief for small tasks).
// Severity configurable per project ("scopeReminder" key of .drwil/ia-first.json, default "warning"):
// "blocking" fails the commit; "warning" shows without ever blocking; "off" disables the check.
// This setting only touches this check, never the rest of check-docs.
function checkScopeReminder() {
  const errors = [];
  let indexed;
  try {
    indexed = execFileSync("git", ["ls-files"], { cwd: root, stdio: ["ignore", "pipe", "ignore"] })
      .toString()
      .split(/\r?\n/)
      .filter(Boolean);
  } catch {
    return errors; // no git repository, or git missing: nothing to check.
  }
  const projectsDir = join(root, cfg.dirs?.projects ?? "docs/projects");
  const briefs = mdRecursive(projectsDir).filter((p) => !MODEL_RE.test(relative(projectsDir, p)));
  const briefTexts = briefs.map((f) => [relative(root, f), readFileSync(f, "utf8")]);
  for (const [rel, text] of briefTexts) {
    const { problems } = scope.readBlock(text);
    for (const p of problems) {
      if (p.kind === "header") errors.push(T.scopeHeader(rel));
      else if (p.kind === "line") errors.push(T.scopeLine(rel, p.line));
      else if (p.kind === "broad") errors.push(T.scopeBroad(rel, p.pattern));
    }
  }
  const byBrief = scope.patternsByBrief(briefTexts, cfg);
  for (const c of scope.outsideBriefs(indexed, byBrief, cfg)) errors.push(T.outsideBriefs(c));
  return errors;
}

function checkBrief(brief, errors, isProject) {
  const rel = relative(root, brief);
  const text = readFileSync(brief, "utf8");
  const lines = text.split(/\r?\n/);
  const start = lines.slice(0, 15).findIndex((l) => STATUS_RE.test(l));
  if (start === -1 || !DATE_RE.test(lines.slice(start, start + 4).join(" "))) errors.push(T.status(rel));
  if (isProject) {
    if (briefStatus(text) === "done") errors.push(T.briefDone(rel));
    if (briefMaturity(text) === "ready") for (const gap of readyGaps(text)) errors.push(T[gap](rel));
    // the model numbers its sections ("## 7. Hand-off"): detection must tolerate that prefix.
    if (!lines.some((l) => /^##\s*(\d+\.\s*)?(Reprise|Hand-off)\b/.test(l))) errors.push(T.handOff(rel));
  } else {
    for (const names of INTENT_SECTIONS) {
      if (!names.some((s) => lines.some((l) => l.startsWith(s)))) errors.push(T.section(rel, names[lang === "en" ? 1 : 0].slice(3)));
    }
  }
}
