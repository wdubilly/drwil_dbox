#!/usr/bin/env node
// Glossary of the identifiers a human reads (QUA-039): a contract ID, a catalog line, a lot or a
// numbered point of the active brief, cited alone, tells the human nothing. A single source, read by
// the checks' outputs, by any agent (`node .githooks/glossary.mjs "<text>"`) and by the agent-side
// hooks: the labels are read from the registry, the catalog and the active brief, never copied.
// Targeted reading (QUA-124, QUA-125): a contract's full text by its ID, the list of every contract, a
// journal entry by its workstream — what an agent needs, without reading the whole file into its context.
import { existsSync, readdirSync, readFileSync, realpathSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { readContracts } from "./contracts.mjs";

const MAX = 15;
const SHORT = 90;
const short = (s) => (s.length > SHORT ? `${s.slice(0, SHORT - 1).trimEnd()}…` : s);
const read = (p) => (existsSync(p) ? readFileSync(p, "utf8") : "");

function config(root) {
  try {
    return JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8")) ?? {};
  } catch {
    return {};
  }
}
const contracts = (root, cfg = config(root)) => readContracts(read(join(root, cfg.dirs?.contracts ?? "docs/contracts.md")));

/** Labels known in `root`: `Map` from the identifier as cited (« QUA-015 », « catalog:QUA-004 », « lot 3 », « point 12 ») to its label. */
export function glossary(root) {
  const cfg = config(root);
  const labels = new Map();
  for (const k of contracts(root, cfg)) if (k.title) labels.set(k.id, short(k.title));
  for (const m of read(join(root, cfg.dirs?.catalog ?? "docs/contracts-catalog.md")).matchAll(
    /^\|\s*(catal(?:ogue|og):[A-Z][A-Z0-9]*-\d+)\s*\|\s*([^|]+)\|/gm,
  ))
    labels.set(m[1], short(m[2].trim()));
  let brief = null;
  try {
    brief = JSON.parse(readFileSync(join(root, ".drwil", "state.json"), "utf8"))?.active_brief;
  } catch {}
  const text = typeof brief === "string" ? read(join(root, brief)) : "";
  for (const m of text.matchAll(/^\s*-\s*\*\*(?:Lot|Increment|Incrément)\s+(\d+[a-z]?)\s*[—–-]\s*([^*]+)\*\*/gim))
    labels.set(`lot ${m[1]}`, short(m[2].trim()));
  for (const m of text.matchAll(/^\s*(\d+)\.\s+\*\*([^*]+)\*\*/gm)) labels.set(`point ${m[1]}`, short(m[2].trim()));
  // Working documents: the value discovery's opportunities (its French or English name)…
  for (const name of ["docs/decouverte-valeur.md", "docs/value-discovery.md"])
    for (const m of read(join(root, name)).matchAll(/^\|\s*(OPT-\d+)\s*\|\s*([^|]+)\|/gm)) labels.set(m[1], short(m[2].trim()));
  // …and the markers an intention or a brief declares in its « Repères » / « Markers » section.
  for (const dir of [cfg.dirs?.intentions ?? "docs/intentions", cfg.dirs?.projects ?? "docs/projects"])
    for (const file of markdown(join(root, dir))) {
      const section = /^##\s+(?:Repères|Markers)\s*$([\s\S]*?)(?=^##\s|(?![\s\S]))/m.exec(read(file));
      for (const m of section?.[1].matchAll(/^\s*-\s*`([^`]+)`\s*[—–-]\s*(.+)$/gm) ?? []) labels.set(m[1], short(m[2].trim()));
    }
  return labels;
}

/** The Markdown files of a folder (not recursive); none if it is missing. */
function markdown(dir) {
  try {
    return readdirSync(dir)
      .filter((f) => f.endsWith(".md"))
      .map((f) => join(dir, f));
  } catch {
    return [];
  }
}

const escapeRe = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
/** Pattern of an identifier as it is written in a text (lots and points in either language, any case). */
function pattern(id) {
  const m = /^(lot|point) (.+)$/.exec(id);
  if (m) return new RegExp(`\\b(?:${m[1] === "lot" ? "lot|increment" : "point"})\\s+${escapeRe(m[2])}\\b`, "i");
  // Not inside a longer ID, nor inside a catalog reference (« catalog:QUA-004 » is the catalog's line).
  return new RegExp(`(?<![A-Za-z0-9:-])${escapeRe(id)}(?![0-9])`);
}

/** The known identifiers cited in `text`, each with its label: `[{ id, label }]`, at most fifteen. */
export function cited(text, labels) {
  const found = [];
  for (const [id, label] of labels) {
    if (pattern(id).test(text)) found.push({ id, label });
    if (found.length >= MAX) break;
  }
  return found;
}

// A gloss right after the ID, on the same line: a parenthesis, a dash, a colon, or the next table cell
// (code or bold marks around the ID allowed).
const GLOSS = /^[`*]*\s*[(—–:|]/;

/**
 * The known identifiers of `text` cited bare: an ID whose mentions are not glossed (GLOSS) before a
 * glossed one — once labelled, later bare mentions are fine. `[{ id, label }]`, at most fifteen.
 */
export function unlabelled(text, labels) {
  const found = [];
  for (const [id, label] of labels) {
    // Only the first mention matters: glossed, the later ones are fine; bare, the ID is bare.
    const m = pattern(id).exec(text);
    if (m && !GLOSS.test(text.slice(m.index + m[0].length))) found.push({ id, label });
    if (found.length >= MAX) break;
  }
  return found;
}

/** `text` with the first mention of each known identifier followed by its label, unless it already is. */
export function annotate(text, labels) {
  let out = text;
  for (const { id, label } of cited(text, labels)) {
    // `cited` found it in `text`, and an annotation never removes an ID: it is still there.
    const m = pattern(id).exec(out);
    const end = m.index + m[0].length;
    if (out.slice(end, end + label.length + 6).includes(label.slice(0, 20))) continue;
    // Already inside parentheses (« (QUA-004) »): a dash, not nested parentheses.
    out = `${out.slice(0, end)}${out[end] === ")" ? ` — ${label}` : ` (${label})`}${out.slice(end)}`;
  }
  return out;
}

/** Full text of the contract `id` in the registry (heading and fields, as written), or null. */
export function contractText(root, id) {
  return (
    contracts(root)
      .find((k) => k.id === id)
      ?.text.trimEnd() ?? null
  );
}

/** Every contract of the registry, one line each (`ID — title`), none left out, titles never shortened. */
export const contractList = (root) => contracts(root).map((k) => (k.title ? `${k.id} — ${k.title}` : k.id));

/**
 * The journal entries of a workstream (`## ` sections): those citing its closing proof or its branch,
 * otherwise those whose title contains `name` (any case). Empty when nothing matches.
 */
export function journalEntries(root, name) {
  const cfg = config(root);
  const entries = read(join(root, cfg.dirs?.projects ?? "docs/projects", "journal.md"))
    .split(/^(?=## )/m)
    .filter((e) => e.startsWith("## "))
    .map((e) => e.trimEnd());
  const slug = escapeRe(name);
  const cites = new RegExp(`${escapeRe(cfg.dirs?.proofs ?? "docs/preuves")}/${slug}\\.md|(?:workstream|chantier)/${slug}(?![\\w.-])`);
  const byWorkstream = entries.filter((e) => cites.test(e));
  if (byWorkstream.length) return byWorkstream;
  const lower = name.toLowerCase();
  return entries.filter((e) => e.split("\n", 1)[0].toLowerCase().includes(lower));
}

const TEXTS = {
  fr: {
    unknown: (id) => `contrat ${id} absent du registre (liste : node .githooks/glossary.mjs --contracts)`,
    noEntry: (n) => `aucune entrée du journal pour « ${n} » (ni sa preuve, ni sa branche, ni son titre)`,
    usage: "usage : node .githooks/glossary.mjs [<texte>] | --contract <ID> | --contracts | --journal <chantier>",
  },
  en: {
    unknown: (id) => `contract ${id} not in the registry (list: node .githooks/glossary.mjs --contracts)`,
    noEntry: (n) => `no journal entry for "${n}" (neither its proof, nor its branch, nor its title)`,
    usage: "usage: node .githooks/glossary.mjs [<text>] | --contract <ID> | --contracts | --journal <workstream>",
  },
};

/** The targeted readings (`--contract`, `--contracts`, `--journal`): exit code, or null for another command. */
function targeted(args, root, log) {
  const T = TEXTS[config(root).lang === "en" ? "en" : "fr"];
  const [option, value] = args;
  if (option === "--contracts") {
    for (const line of contractList(root)) log(line);
    return 0;
  }
  if (option !== "--contract" && option !== "--journal") return null;
  if (!value) {
    log(T.usage);
    return 2;
  }
  const found = option === "--contract" ? [contractText(root, value)].filter(Boolean) : journalEntries(root, value);
  if (!found.length) {
    log(option === "--contract" ? T.unknown(value) : T.noEntry(value));
    return 1;
  }
  log(found.join("\n\n"));
  return 0;
}

/**
 * Runs the command: a targeted reading (above), or the labels of the identifiers cited in the arguments
 * (or standard input); returns the exit code.
 */
export function main(args = process.argv.slice(2), root = process.cwd(), log = console.log, input = () => readFileSync(0, "utf8")) {
  const code = targeted(args, root, log);
  if (code !== null) return code;
  let text = args.join(" ");
  if (!args.length) {
    try {
      text = input();
    } catch {}
  }
  for (const { id, label } of cited(text, glossary(root))) log(`${id} — ${label}`);
  return 0;
}

// Through a symbolic link (an installed copy), argv[1] is the link: compare the real paths. The root
// comes from the path called, not the current folder (an agent's `cd`): the parent of its .githooks/.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  const here = dirname(resolve(process.argv[1]));
  process.exitCode = main(process.argv.slice(2), basename(here) === ".githooks" ? join(here, "..") : process.cwd());
}
