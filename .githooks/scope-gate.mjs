#!/usr/bin/env node
// Scope gate (docs/ia-first.md, "Scope gate"), the engine's "scope-gate" check
// (.githooks/engine.mjs): commit, push, CI, `drwil verify`.
//
// - Briefs (docs/projects/, docs/intentions/…) can always be committed.
// - At commit time, any other file requires the BUILD activity and must be covered by the
//   scope block of the active brief as it is in HEAD: widening its scope takes a separate
//   commit, visible on its own. During a merge, only the resolution is judged (QUA-111).
// - Elsewhere (pre-push, CI, verify), there is no local state: each commit of the branch is
//   replayed against the scope of the brief named by its `Drwil-Brief` trailer, read in its
//   parent. A commit is judged by its parent's rules: without a gate in the parent, it is not
//   checked.
// Severity: `scopeGate` setting (off | warning | blocking), warning if missing.
import { spawnSync } from "node:child_process";
import { existsSync, realpathSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { contractsAddedOnly, isRegistry, rulesFiles } from "./detour.mjs";
import { fnmatch, normalize, readBlock, withMirrors } from "./scope.mjs";
import { governanceDisabled, readState } from "./state.mjs";

// Attestations pass like briefs: a human proof already guarded by `drwil attest` (terminal
// required, contract fingerprint), reviewed; subjecting them to the scope would force widening
// every brief in order to close. The consumption log likewise: its line is written in CLOSING,
// for the closing commit (`consumption.mjs --record`). The closing proof likewise (QUA-084): the
// closing writes it in CLOSING, for the closing commit, and refuses to close without it.
// The scoping recipes' outputs (value discovery, risk audit — French and English names) are scoping
// work, written in SCOPING: exact names, never a broad pattern on docs/. The release notes
// likewise (`docs/recettes/`): written by `recette-note.mjs` and committed by git plumbing, with no
// hook, so no trailer, while no brief is active; reviewed in their pull request.
export const OUT_OF_SCOPE = [
  "docs/projects/*",
  "docs/intentions/*",
  ".drwil/evidence/attestations/*",
  ".drwil/usage.jsonl",
  "docs/preuves/*",
  "docs/recettes/*",
  "docs/value-discovery.md",
  "docs/decouverte-valeur.md",
  "docs/audit-risks.md",
  "docs/audit-risques.md",
];
export const TRAILER = "Drwil-Brief";
const LEVELS = new Set(["off", "warning", "blocking"]);
const MARKER = ".githooks/scope-gate.mjs";

const TEXTS = {
  fr: {
    activity: (a, n) =>
      `${n} fichier(s) hors fiches commité(s) en ${a} : seule l'activité BUILD l'autorise (node .githooks/state.mjs move …)`,
    outsideScope: (f, brief) => `${f} : hors du cadrage de ${brief} (dans HEAD) — l'ajouter au bloc scope et commiter la fiche d'abord`,
    noTrailer: (c, n) => `${c} : ${n} fichier(s) hors fiches sans trailer ${TRAILER}`,
    outsideScopeCommit: (c, f, brief) => `${c} : ${f} hors du cadrage de ${brief} (dans le parent)`,
    noReference: "aucune branche de référence (origin/HEAD, master, main, ou DRWIL_BASE)",
    failed: (n) => `périmètre de l'attente : ${n} problème(s) (barrière bloquante)`,
    rules: (f) =>
      `${f} : un détour qui change les règles du jeu (contrat existant modifié ou retiré, hook, seuil, barrière, CI) demande le oui de l'humain — \`node .githooks/detour.mjs allow\` dans un terminal ; en file, le sous-agent s'arrête et le signale`,
    reopenRules: (f) =>
      `${f} : un correctif de réouverture qui change les règles du jeu (contrat existant modifié ou retiré, hook, seuil, barrière, CI) demande le oui de l'humain — \`node .githooks/detour.mjs allow\` dans un terminal ; en file, le sous-agent s'arrête et le signale`,
  },
  en: {
    activity: (a, n) =>
      `${n} file(s) outside briefs committed in ${a}: only the BUILD activity allows it (node .githooks/state.mjs move …)`,
    outsideScope: (f, brief) => `${f}: outside the scope of ${brief} (in HEAD) — add it to the scope block and commit the brief first`,
    noTrailer: (c, n) => `${c}: ${n} file(s) outside briefs without a ${TRAILER} trailer`,
    outsideScopeCommit: (c, f, brief) => `${c}: ${f} outside the scope of ${brief} (in the parent)`,
    noReference: "no reference branch (origin/HEAD, master, main, or DRWIL_BASE)",
    failed: (n) => `brief scope: ${n} problem(s) (blocking gate)`,
    rules: (f) =>
      `${f}: a detour that changes the rules (an existing contract modified or removed, hook, threshold, gate, CI) needs the human's yes — \`node .githooks/detour.mjs allow\` in a terminal; in a queue, the subagent stops and reports it`,
    reopenRules: (f) =>
      `${f}: a reopening fix that changes the rules (an existing contract modified or removed, hook, threshold, gate, CI) needs the human's yes — \`node .githooks/detour.mjs allow\` in a terminal; in a queue, the subagent stops and reports it`,
  },
};

const texts = (cfg) => TEXTS[cfg?.lang === "en" ? "en" : "fr"];

function git(root, args) {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout : null;
}

/** Files subject to the scope: everything but briefs (configurable list). */
export function outsideBriefFiles(files, outOfScope = OUT_OF_SCOPE) {
  return files.map(normalize).filter((f) => !outOfScope.some((p) => fnmatch(f, p)));
}

const covered = (f, patterns) => patterns.some((p) => fnmatch(f, p));

/**
 * Problems of a commit being prepared (pure). `addedOnly(f)`: true when the staged registry `f` only
 * adds new contracts (read by checkScope); without it, any registry change waits for the yes.
 */
export function checkIndex({ files, state, patterns, outOfScope, cfg, addedOnly = () => false }) {
  const T = texts(cfg);
  const subject = outsideBriefFiles(files, outOfScope);
  if (!subject.length) return [];
  if (state.activity !== "BUILD") return [T.activity(state.activity, subject.length)];
  const problems = subject.filter((f) => !covered(f, patterns)).map((f) => T.outsideScope(f, state.active_brief));
  // QUA-050, QUA-117: an ordinary detour or reopening fix needs no one, nor does adding a contract; one
  // that changes the rules (an existing contract modified or removed among them) waits for the human's yes.
  const exception = state.detour ?? state.reopened;
  if (exception && exception.human !== true)
    problems.push(
      ...rulesFiles(subject, cfg)
        .filter((f) => !addedOnly(f))
        .map(state.detour ? T.rules : T.reopenRules),
    );
  return problems;
}

/** Patterns of the scope block of `brief` in revision `rev`, copies included; none if the brief does not exist there. */
function patternsAt(root, rev, brief, cfg) {
  const text = brief ? git(root, ["show", `${rev}:${brief}`]) : null;
  return text === null ? [] : withMirrors(readBlock(text).patterns, cfg);
}

function reference(root, env) {
  const candidates = env.DRWIL_BASE ? [env.DRWIL_BASE] : [];
  const origin = git(root, ["symbolic-ref", "--quiet", "refs/remotes/origin/HEAD"]);
  if (origin) candidates.push(origin.trim());
  candidates.push("origin/master", "origin/main", "master", "main");
  for (const ref of candidates) {
    if (git(root, ["rev-parse", "--verify", "--quiet", `${ref}^{commit}`]) === null) continue;
    const base = git(root, ["merge-base", "HEAD", ref]);
    if (base) return base.trim();
  }
  return null;
}

function checkBranch(root, base, { outOfScope, cfg }) {
  const T = texts(cfg);
  const problems = [];
  // No `?? ""` fallback on these git calls: they work on existing commits (base from merge-base);
  // a failure must break the check, not let it pass without judging anything (QUA-013).
  const commits = git(root, ["rev-list", "--no-merges", "--reverse", `${base}..HEAD`])
    .split("\n")
    .filter(Boolean);
  for (const c of commits) {
    const short = c.slice(0, 7);
    // The parent's rules: without a gate in the parent, the commit is not judged.
    if (git(root, ["cat-file", "-e", `${c}^:${MARKER}`]) === null) continue;
    const files = git(root, ["diff-tree", "--no-commit-id", "--name-only", "-r", "--no-renames", c]).split("\n").filter(Boolean);
    const subject = outsideBriefFiles(files, outOfScope);
    if (!subject.length) continue;
    const values = git(root, ["log", "-1", `--format=%(trailers:key=${TRAILER},valueonly)`, c])
      .split("\n")
      .map((v) => v.trim())
      .filter(Boolean);
    const brief = values.at(-1);
    if (!brief) {
      problems.push(T.noTrailer(short, subject.length));
      continue;
    }
    const patterns = patternsAt(root, `${c}^`, brief, cfg);
    for (const f of subject) if (!covered(f, patterns)) problems.push(T.outsideScopeCommit(short, f, brief));
  }
  return problems;
}

/** `mode sha` of `f` in `rev` (null: the index); "" when absent; null when git fails. */
function entry(root, rev, f) {
  const args = rev ? ["ls-tree", "-z", "--full-tree", rev, "--", f] : ["ls-files", "--stage", "-z", "--", f];
  const out = git(root, ["--literal-pathspecs", ...args]);
  if (out === null) return null;
  const line = out.split("\0").find((l) => l.endsWith(`\t${f}`));
  if (!line) return "";
  // ls-tree: `mode type sha`; ls-files --stage: `mode sha stage`.
  const fields = line.split("\t")[0].split(" ");
  return `${fields[0]} ${fields[rev ? 2 : 1]}`;
}

/**
 * QUA-111: during a merge (MERGE_HEAD), only the resolution is the author's: a staged file identical to
 * one of the two parents (deleted and absent there included) is brought as it is, not judged. A failed
 * git call keeps the file judged, never lets it pass (QUA-013).
 */
function resolution(root, staged) {
  if (git(root, ["rev-parse", "--verify", "--quiet", "MERGE_HEAD^{commit}"]) === null) return staged;
  return staged.filter((f) => {
    const own = entry(root, null, f);
    if (own === null) return true;
    return !["HEAD", "MERGE_HEAD"].some((rev) => entry(root, rev, f) === own);
  });
}

/**
 * Full check for the engine: `{ status, problems, message?, detail? }`.
 * `staged` (staged files) is only given at pre-commit; otherwise the branch is replayed.
 */
export function checkScope(root, { cfg = {}, staged = null, env = process.env } = {}) {
  const level = LEVELS.has(cfg.scopeGate) ? cfg.scopeGate : "warning";
  if (level === "off") return { status: "not-applicable", problems: [], detail: "scopeGate: off" };
  const outOfScope = Array.isArray(cfg.outOfScope) ? cfg.outOfScope : OUT_OF_SCOPE;
  let problems;
  if (staged) {
    const { state } = readState(root);
    // The staged registry against HEAD's: `git show` fails (null) on a registry created or removed — a doubt.
    const addedOnly = (f) => isRegistry(f, cfg) && contractsAddedOnly(git(root, ["show", `HEAD:${f}`]), git(root, ["show", `:${f}`]));
    problems = checkIndex({
      files: resolution(root, staged),
      state,
      patterns: patternsAt(root, "HEAD", state.active_brief, cfg),
      outOfScope,
      cfg,
      addedOnly,
    });
  } else {
    const base = reference(root, env);
    if (!base) return { status: "not-run", problems: [], detail: texts(cfg).noReference, configured: true };
    problems = checkBranch(root, base, { outOfScope, cfg });
  }
  if (problems.length && level === "blocking") return { status: "fail", problems, message: texts(cfg).failed(problems.length) };
  return { status: "ok", problems };
}

/** True while git replays existing commits (rebase): their message, trailer included, is kept as it is. */
function replaying(root) {
  return ["rebase-merge", "rebase-apply"].some((name) => {
    const path = git(root, ["rev-parse", "--git-path", name]);
    return path !== null && existsSync(resolve(root, path.trim()));
  });
}

/**
 * Adds `Drwil-Brief: <active brief>` to the message (prepare-commit-msg hook); nothing without a brief or
 * governance, nor during a rebase: a replayed commit keeps the brief it was made under (a detour's resume
 * rebases the workstream's branch while the state still names the detour's brief).
 */
export function addTrailer(root, messageFile) {
  if (governanceDisabled(root) || replaying(root)) return;
  const { state } = readState(root);
  if (!state.active_brief) return;
  git(root, ["interpret-trailers", "--in-place", "--if-exists", "replace", "--trailer", `${TRAILER}: ${state.active_brief}`, messageFile]);
}

// `node .githooks/scope-gate.mjs --trailer <file>` (prepare-commit-msg). Never blocks the commit:
// addTrailer does not throw, a git failure leaves the message as it is.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  const i = process.argv.indexOf("--trailer");
  if (i >= 0 && process.argv[i + 1]) addTrailer(process.cwd(), process.argv[i + 1]);
}
