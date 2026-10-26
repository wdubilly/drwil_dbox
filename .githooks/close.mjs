#!/usr/bin/env node
// Closing of a workstream in one gesture (QUA-048): `node .githooks/close.mjs` (in CLOSING) deletes
// the brief, removes its index line and records the consumption; `--check` (any activity) only
// reports, before the commit, what would make the closing fail. It never stages, commits, pushes nor
// merges anything, and it never writes the state: the journal entry and the limits stay the agent's.
// The recognition and deletion of the merged local branches live in close-branches.mjs (QUA-048,
// QUA-058, QUA-059), re-exported here.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, realpathSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, posix } from "node:path";
import { fileURLToPath } from "node:url";
import { undeclaredHooks } from "./base-checks.mjs";
import { acceptanceCriteria, config } from "./briefs.mjs";
import { changedFiles } from "./check-code-rules.mjs";
import { main as consumption } from "./consumption.mjs";
import { definedIds } from "./contracts.mjs";
import {
  applyProposals,
  checkboxProblems,
  codeCitations,
  deadReferences as deadCitations,
  indexItem,
  intentLeft,
  proposals,
  twinIntents,
} from "./doc-coherence.mjs";
import { closureProof, proofNotCited, proofPath, proofText, writeProof } from "./closure-proof.mjs";
import { mainBranch } from "./close-branches.mjs";
import { lateLines } from "./worktree.mjs";
import { readState, stateRoot, verifyEvidence } from "./state.mjs";

// Compatibility: the branches' functions live in close-branches.mjs, importers keep this path.
export {
  branchMerged,
  closingMerged,
  ghRun,
  mainBranch,
  mergedPullRequests,
  noGhText,
  prunedMessage,
  pruneMergedBranches,
} from "./close-branches.mjs";

const TEXTS = {
  fr: {
    usage: "usage : node .githooks/close.mjs [--check] [--fix] [<chemin de la fiche>] [--logs <dossier>]",
    noBrief: "clôture : aucune fiche active ni fiche donnée",
    notClosing: (a) =>
      `clôture refusée : activité ${a} — la mécanique de clôture se fait en CLOSING (\`--check\` signale sans rien écrire)`,
    path: (steps) => `Chemin jusqu'à CLOSING, dans l'ordre :\n${steps.map((s) => `  ${s}`).join("\n")}`,
    rerun: "`npx drwil verify --evidence` (l'évidence ne date pas de HEAD : la renouveler une fois le travail commité)",
    deleted: (b) => `fiche supprimée : ${b}`,
    indexLine: (f) => `ligne d'index retirée : ${f}`,
    noIndexLine: (f, b) =>
      `aucune ligne d'index à retirer pour ${b} dans ${f} (ligne absente, ou commune à plusieurs fiches : à retirer à la main)`,
    toCreate: (c, b, id) => `critère « ${c} » encore « contrat à créer » : écrire, entre backticks à la fin de ce critère (${b}), ${id}`,
    candidates: (ids) => (ids.length ? `l'ID du contrat créé (${ids.join(" ou ")})` : "l'ID du contrat, une fois créé dans le registre"),
    dead: (place, b) => `${place} cite ${b}, qui sera supprimée : citer le contrat ou le journal à la place`,
    bare: (place, n) => `${place} : \`${n}\` sans chemin, refusé par check-docs — écrire son chemin depuis la racine`,
    signals: "Avant le commit, ce qui ferait échouer la clôture :",
    byHand: "à corriger à la main (la case dépend de plusieurs fiches, ou la ligne n'a pas de marqueur)",
    clean: "Rien ne ferait échouer la clôture.",
    proposed: "Corrections proposées, rien n'est écrit (`node .githooks/close.mjs --fix` les applique) :",
    applied: (n) => `corrections appliquées : ${n}`,
    warnings: "Avertissements (non bloquants) :",
    rest: "Reste à l'agent : l'entrée du journal (décisions, limites en intentions), puis le commit de clôture.",
    twice: (j, f, n) =>
      `${j} : ${n} entrées citent \`${f}\` — un chantier rouvert met à jour son entrée de journal, il n'en ajoute pas une seconde`,
  },
  en: {
    usage: "usage: node .githooks/close.mjs [--check] [--fix] [<brief path>] [--logs <folder>]",
    noBrief: "closing: no active brief and no brief given",
    notClosing: (a) => `closing refused: activity ${a} — the closing mechanics run in CLOSING (\`--check\` reports without writing)`,
    path: (steps) => `Path to CLOSING, in order:\n${steps.map((s) => `  ${s}`).join("\n")}`,
    rerun: "`npx drwil verify --evidence` (the evidence is not from HEAD: renew it once the work is committed)",
    deleted: (b) => `brief deleted: ${b}`,
    indexLine: (f) => `index line removed: ${f}`,
    noIndexLine: (f, b) => `no index line to remove for ${b} in ${f} (line missing, or shared by several briefs: remove it by hand)`,
    toCreate: (c, b, id) =>
      `criterion « ${c} » still « contract to create »: write, in backticks at the end of this criterion (${b}), ${id}`,
    candidates: (ids) =>
      ids.length ? `the ID of the contract created (${ids.join(" or ")})` : "the contract's ID, once created in the registry",
    dead: (place, b) => `${place} cites ${b}, which will be deleted: cite the contract or the journal instead`,
    bare: (place, n) => `${place}: \`${n}\` without a path, refused by check-docs — write its path from the root`,
    signals: "Before the commit, what would make the closing fail:",
    byHand: "to fix by hand (the box depends on several briefs, or the line has no marker)",
    clean: "Nothing would make the closing fail.",
    proposed: "Proposed corrections, nothing is written (`node .githooks/close.mjs --fix` applies them):",
    applied: (n) => `corrections applied: ${n}`,
    warnings: "Warnings (non-blocking):",
    rest: "Left to the agent: the journal entry (decisions, limits as intents), then the closing commit.",
    twice: (j, f, n) => `${j}: ${n} entries cite \`${f}\` — a reopened workstream updates its journal entry, it never adds a second one`,
  },
};

const texts = (root) => TEXTS[config(root).lang === "en" ? "en" : "fr"];

const git = (root, ...args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout.trim() : null;
};

/** Contract IDs the project defines in `text`, per its prefixes. */
function contractIds(root, text) {
  const prefixes = config(root).contractPrefixes ?? ["SEC", "QUA"];
  return definedIds(text, `(?:${prefixes.join("|")})-\\d{3}\\b`).defined;
}

/** Criteria still « contract to create », with the ID to write: the contracts created on the branch the brief does not cite yet. */
function criteriaToCreate(root, brief, text) {
  const T = texts(root);
  const open = acceptanceCriteria(text).filter((c) => !c.link && /contrat à créer|contract to create/i.test(c.text));
  if (!open.length) return [];
  const registry = config(root).dirs?.contracts ?? "docs/contracts.md";
  const now = existsSync(join(root, registry)) ? readFileSync(join(root, registry), "utf8") : "";
  const main = mainBranch(root);
  const base = main ? git(root, "merge-base", "HEAD", main) : null;
  const before = contractIds(root, (base && git(root, "show", `${base}:${registry}`)) ?? "");
  const ids = [...contractIds(root, now)].filter((id) => !before.has(id) && !text.includes(id));
  // One contract per criterion, created in the same order: the pairing is given; otherwise the candidates.
  const short = (t) => {
    const label = t.replace(/\s*[—–-]\s*(contrat à créer|contract to create)\s*$/i, "");
    return label.length > 70 ? `${label.slice(0, 69)}…` : label;
  };
  return open.map((c, i) => T.toCreate(short(c.text), brief, ids.length === open.length ? `\`${ids[i]}\`` : T.candidates(ids)));
}

/** A single journal entry per workstream (QUA-118): its proof cited by two `## ` entries is an entry doubled. */
function journalTwice(root, brief) {
  const journal = `${config(root).dirs?.projects ?? "docs/projects"}/journal.md`;
  if (!existsSync(join(root, journal))) return [];
  const proof = proofPath(root, brief);
  const n = readFileSync(join(root, journal), "utf8")
    .split(/^## /m)
    .slice(1)
    .filter((entry) => entry.includes(proof)).length;
  return n > 1 ? [texts(root).twice(journal, proof, n)] : [];
}

const USAGE = [".drwil", "usage.jsonl"];
const recorded = (text, name) =>
  text.split("\n").some((l) => {
    try {
      return JSON.parse(l).workstream === name;
    } catch {
      return false;
    }
  });

/**
 * Records the consumption; for a reopened workstream (QUA-118), its line is replaced by the new total
 * (the logs are read again in full), never doubled, and kept as it was when nothing is measured.
 */
function recordConsumption(root, name, { reopened, env, logs, log }) {
  const file = join(root, ...USAGE);
  const before = reopened && existsSync(file) ? readFileSync(file, "utf8") : null;
  const replace = before !== null && recorded(before, name);
  if (replace)
    writeFileSync(
      file,
      before
        .split("\n")
        .filter((l) => !recorded(l, name))
        .join("\n"),
    );
  consumption(["--record", name, ...(logs ? ["--logs", logs] : [])], { env, root, log, err: log });
  if (replace && !recorded(readFileSync(file, "utf8"), name)) writeFileSync(file, before);
}

/** Places (file:line) of the non-Markdown files citing the brief (the Markdown ones are corrected by `deadCitations`). */
function deadReferences(root, brief) {
  const T = texts(root);
  const out = git(root, "grep", "-n", "-I", "--untracked", "-F", brief) ?? "";
  return out
    .split("\n")
    .filter(Boolean)
    .map((l) => /^(.+?):(\d+):/.exec(l))
    .filter(([, f]) => f !== brief && !f.endsWith(".md"))
    .map(([, f, n]) => T.dead(`${f}:${n}`, brief));
}

const NAME = /`([^`\s/<>{}*?|=:@$]+\.(?:md|mdc|mjs|cjs|js|ts|tsx|jsx|py|sh|json|ya?ml|toml|txt))`(?!\s*\((?:à créer|to create)\))/g;

/** Backticked file names without a path, in the Markdown files changed, that check-docs would not find. */
function bareNames(root) {
  const T = texts(root);
  const found = [];
  for (const file of changedFiles(root).filter((f) => f.endsWith(".md") && existsSync(join(root, f)))) {
    const lines = readFileSync(join(root, file), "utf8").split(/\r?\n/);
    lines.forEach((line, i) => {
      if (/si présent|if present/i.test(line + (lines[i + 1] ?? ""))) return;
      for (const [, name] of line.matchAll(NAME)) {
        if (!name.startsWith("-") && !existsSync(join(root, name)) && !existsSync(join(root, dirname(file), name)))
          found.push(T.bare(`${file}:${i + 1}`, name));
      }
    });
  }
  return found;
}

/**
 * What would make the closing of `brief` fail and what it only warns about; `text`: the brief, read before it
 * is deleted. `problems`: `{ message }`, with the line rewrite (`file`, `line`, and `edit` or `fixTo`) when
 * the correction needs no judgement; the rules shared with check-docs live in doc-coherence.mjs.
 */
export function closingReport(root, brief, text = existsSync(join(root, brief)) ? readFileSync(join(root, brief), "utf8") : "") {
  const cfg = config(root);
  const lang = cfg.lang === "en" ? "en" : "fr";
  const T = texts(root);
  const plain = [
    ...criteriaToCreate(root, brief, text),
    ...undeclaredHooks(root, text).lines,
    ...proofNotCited(root, brief),
    ...journalTwice(root, brief),
    ...deadReferences(root, brief),
    ...bareNames(root),
  ].map((message) => ({ message }));
  const reported = new Set(plain.map((p) => /^(\S+:\d+) /.exec(p.message)?.[1]));
  const code = codeCitations(root, cfg, lang).filter((c) => !reported.has(c.place));
  const boxes = checkboxProblems(root, cfg, lang, { except: brief }).map((p) => ({
    ...p,
    message: `${p.place} : ${p.message}${p.fixTo ? "" : ` — ${T.byHand}`}`,
  }));
  const problems = [
    ...plain,
    ...deadCitations(root, cfg, lang, brief),
    ...boxes,
    ...code,
    ...intentLeft(root, lang, brief, text).map((message) => ({ message })),
  ];
  return { problems, warnings: twinIntents(root, cfg, lang) };
}

/** What would make the closing of `brief` fail, as messages. */
export const closingSignals = (root, brief, text) => closingReport(root, brief, text).problems.map((p) => p.message);

const STEPS = ["BUILD", "EVIDENCE", "VERIFY", "CLOSING"];

/**
 * The commands that remain from `activity` to CLOSING (neighbouring transitions only), the evidence
 * renewal placed before the last one when it is absent or from another commit than HEAD; null outside
 * a started workstream, where closing is not in the agent's hands.
 */
export function pathToClosing(root, activity) {
  const at = STEPS.indexOf(activity);
  if (at < 0 || at === STEPS.length - 1) return null;
  const T = texts(root);
  const steps = STEPS.slice(at + 1).map((a) => `node .githooks/state.mjs move ${a}`);
  const evidence = verifyEvidence(root);
  if (!evidence.ok && (evidence.code === "noEvidence" || evidence.code === "otherCommit")) steps.splice(steps.length - 1, 0, T.rerun);
  return T.path(steps);
}

/**
 * `{ code, message }`. Without `check`: the mechanics (CLOSING only) — brief deleted, index item
 * removed, consumption recorded — then the signals. With `check`: the signals only. With `fix`, the
 * corrections that need no judgement are written (and only then); otherwise they are proposed as a diff.
 */
export function close(root, { brief, check = false, fix = false, env = process.env, logs } = {}) {
  const T = texts(root);
  const { state } = readState(root);
  const path = brief ? posix.normalize(brief.replace(/\\/g, "/")) : state.active_brief;
  if (!path) return { code: 1, message: T.noBrief };
  const route = pathToClosing(root, state.activity);
  if (!check && state.activity !== "CLOSING") return { code: 1, message: [T.notClosing(state.activity), route].filter(Boolean).join("\n") };
  const text = existsSync(join(root, path)) ? readFileSync(join(root, path), "utf8") : "";
  const done = [];
  if (!check) {
    const hooks = undeclaredHooks(root, text); // QUA-137: refused before anything is written
    if (hooks.files.length) return { code: 1, message: [T.signals, ...hooks.lines.map((s) => `  ⚠ ${s}`)].join("\n") };
    // The proof first: a brief is never deleted while what it had proven would stay unwritten (QUA-084).
    const proof = closureProof(root, path, text, { head: git(root, "rev-parse", "HEAD") });
    const problem = proof.problem ?? writeProof(root, proof);
    if (problem) return { code: 1, message: proofText(root, "proofRefused", problem) };
    done.push(proofText(root, "proofWritten", proof.file));
    if (existsSync(join(root, path))) {
      unlinkSync(join(root, path));
      done.push(T.deleted(path));
    }
    const index = config(root).dirs?.index ?? "docs/projects/pending.md";
    const lines = existsSync(join(root, index)) ? readFileSync(join(root, index), "utf8").split("\n") : [];
    const item = indexItem(lines, path);
    if (item) {
      lines.splice(item[0], item[1] - item[0]);
      writeFileSync(join(root, index), lines.join("\n"));
      done.push(T.indexLine(index));
    } else done.push(T.noIndexLine(index, path));
    recordConsumption(root, posix.basename(path, ".md"), { reopened: Boolean(state.reopened), env, logs, log: (m) => done.push(m) });
  }
  let report = closingReport(root, path, text);
  let proposed = proposals(root, report.problems);
  if (fix && proposed.length) {
    done.push(T.applied(applyProposals(root, proposed)));
    report = closingReport(root, path, text);
    proposed = proposals(root, report.problems);
  }
  const problems = report.problems.map((p) => p.message);
  const diff = proposed.flatMap((p) => [`  ${p.file}:${p.line}`, `    - ${p.from}`, `    + ${p.to}`]);
  const lines = [
    ...done,
    ...(problems.length ? [T.signals, ...problems.map((s) => `  ⚠ ${s}`)] : [T.clean]),
    ...(diff.length ? [T.proposed, ...diff] : []),
    ...(report.warnings.length ? [T.warnings, ...report.warnings.map((w) => `  ⚠ ${w}`)] : []),
    ...(check ? [...lateLines(root), route] : [T.rest]),
  ];
  return { code: problems.length ? 1 : 0, message: lines.filter(Boolean).join("\n") };
}

/** Entry point: `node .githooks/close.mjs [--check] [<brief>] [--logs <folder>]`. Returns the exit code. */
export function main(args, root, { env = process.env } = {}) {
  const i = args.indexOf("--logs");
  const logs = i >= 0 ? args[i + 1] : undefined;
  const rest = args.filter((a, j) => a !== "--check" && a !== "--fix" && (i < 0 || (j !== i && j !== i + 1)));
  if (rest.some((a) => a.startsWith("-")) || rest.length > 1) {
    console.error(texts(root).usage);
    return 2;
  }
  const r = close(root, { brief: rest[0], check: args.includes("--check"), fix: args.includes("--fix"), env, logs });
  (r.code === 0 ? console.log : console.error)(r.message);
  return r.code;
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = main(process.argv.slice(2), stateRoot());
}
