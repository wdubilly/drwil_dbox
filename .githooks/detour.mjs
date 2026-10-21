#!/usr/bin/env node
// Detour (QUA-050): mid-workstream, a prior defect outside the active brief's scope is fixed on its own
// branch, cut from the main branch, under its own short brief — the workstream's scope never widens
// for it (QUA-019, no bypassing a check; QUA-017, no direct work on the main branch).
// - `open <name> --title <defect> [--files <a,b>]` (BUILD): sets the work in progress aside (named
//   stash, recorded in the state), creates the branch detour/<name> and its brief, points the state at it.
// - `resume` (once the detour's PR is merged, by squash too: `branchMerged` in close.mjs): back to the
//   workstream's branch, rebased on the fixed main branch, the work restored, the state pointed back at the workstream's brief; refused without
//   losing anything if any step fails.
// - `allow` (human, interactive terminal): the yes a detour that changes the rules needs; until then
//   the scope gate refuses its commits touching them (`rulesFiles`).
// It never commits, pushes nor opens the PR; the state is written by `setDetour` (state.mjs) only.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { BRIEF_DIRS, config } from "./briefs.mjs";
import { branchMerged, mainBranch, mergedPullRequests, noGhText } from "./close.mjs";
import { minimumLevel, SENSITIVE_PATHS } from "./risk.mjs";
import { fnmatch, normalize } from "./scope.mjs";
import { readState, setDetour, stateRoot } from "./state.mjs";
import { fill } from "./state-texts.mjs";

// What changes the rules besides the contracts registry: hooks (the gate among them), thresholds and
// checks (.drwil/ia-first.json), CI.
export const RULES = [".githooks/*", ".drwil/ia-first.json", ".github/workflows/*", ".gitlab-ci.yml"];
const JOURNAL = `${BRIEF_DIRS[0]}journal.md`;

const TEXTS = {
  fr: {
    usage: "usage : node .githooks/detour.mjs open <nom> --title <défaut> [--files <a,b>] | resume | allow",
    notBuild: "détour refusé : activité {0} — un détour s'ouvre en BUILD, en plein chantier",
    already: "détour refusé : un détour est déjà en cours ({0}) — le reprendre d'abord",
    name: "détour refusé : nom attendu en minuscules, chiffres et tirets (ex. : date-mal-lue), et --title <défaut>",
    noBranch: "détour refusé : il faut une branche de chantier et une branche principale (main ou master)",
    exists: "détour refusé : {0} ou {1} existe déjà — choisir un autre nom",
    stashFailed: "détour refusé : le travail en cours ne peut pas être mis de côté (conflit non résolu ?) — rien n'a changé",
    switchFailed: "détour refusé : création de la branche impossible ({0}) — rien n'a changé",
    opened: "détour ouvert : branche {0} depuis {1}, fiche {2}{3}.",
    stashed: ", travail en cours mis de côté (stash {0})",
    steps: [
      "Suite, dans l'ordre :",
      "  1. Cadrer la fiche {2} (bloc scope : la correction et son test, rien d'autre) et la commiter seule.",
      "  2. Test d'abord, vu échouer ; puis la correction (recette « corriger un défaut ») ; `node .githooks/run-checks.mjs`.",
      `  3. Entrée de journal (${JOURNAL}) « Détour de {0} : … » citant la branche {1}, puis suppression de la fiche ; commit.`,
      "  4. Push, PR intitulée « Détour de {0} : … », {3}.",
      "  5. PR fusionnée : `node .githooks/detour.mjs resume`.",
    ].join("\n"),
    auto: "fusion automatique programmée (`gh pr merge --auto --merge`)",
    manual: "fusion laissée à l'humain",
    rulesAhead: "⚠ ces fichiers changent les règles du jeu : le oui de l'humain sera demandé (`allow`) — {0}",
    noDetour: "aucun détour en cours",
    dirty: "reprise refusée : arbre modifié — commiter ou retirer d'abord (rien n'a été touché)",
    notMerged: "reprise refusée : {0} n'est pas encore fusionnée dans {1} — attendre la fusion de sa PR",
    briefLeft: "reprise refusée : la fiche {0} est encore dans {1} — la PR du détour la supprime",
    noJournal: `reprise refusée : aucune entrée de ${JOURNAL} ne cite {0} dans {1} — la PR du détour l'ajoute`,
    switchBack: "reprise refusée : retour sur {0} impossible — état inchangé, travail mis de côté intact",
    rebase:
      "reprise refusée : {0} ne se rebase pas sur {1} sans conflit (rebase annulé) — état inchangé, travail mis de côté intact ; un humain tranche",
    stashMissing: "reprise refusée : la mise de côté {0} est introuvable dans `git stash list` — état inchangé",
    restore:
      "reprise refusée : la restauration du travail entre en conflit — arbre remis propre, mise de côté {0} intacte, état inchangé ; un humain tranche",
    resumed: "détour clos : retour sur {1}, rebasée sur {2}, travail restauré ; BUILD sur {0}.",
    forcePush: "La branche était déjà poussée : `git push --force-with-lease`.",
    human: "le oui de l'humain se donne dans un terminal interactif (`node .githooks/detour.mjs allow`) ; en file, le sous-agent s'arrête",
    confirm: "Le détour {0} change les règles du jeu. L'autoriser ? Retapez DETOUR : ",
    refused: "oui non donné : rien n'a changé",
    allowed: "détour {0} autorisé à changer les règles du jeu",
    // {0} title, {1} workstream brief, {2} branch, {3} date, {4} risk, {5} scope lines.
    brief: `# Détour : {0} (détour de {1})

**Statut** : ouvert le {3} (détour de {1}, branche {2}).
**Risque** : {4}
**Maturité** : en cours

<!-- scope
files:
{5}
-->

(cadrage : la correction et son test, rien d'autre ; le cadrage de {1} ne s'élargit jamais pour un détour.)

## 1. Besoin

{0} : défaut préalable rencontré pendant {1}, hors de son cadrage.

## Critères d'acceptation

- {0} : corrigé, gardé par un test de non-régression vu échouer avant la correction

## 4. Décisions

- Un détour s'ouvre sans demander : le contrôle est à la fusion de sa PR. Contrats concernés : QUA-017 (pas de travail direct sur la branche principale), QUA-019 (pas de contournement).

## 7. Reprise

- PR intitulée « Détour de {1} : {0} » ; avant le push, entrée de journal citant la branche {2} et suppression de cette fiche ; PR fusionnée : \`node .githooks/detour.mjs resume\`.
`,
  },
  en: {
    usage: "usage: node .githooks/detour.mjs open <name> --title <defect> [--files <a,b>] | resume | allow",
    notBuild: "detour refused: activity {0} — a detour opens in BUILD, mid-workstream",
    already: "detour refused: a detour is already in progress ({0}) — resume it first",
    name: "detour refused: name expected in lowercase, digits and dashes (e.g. date-misread), and --title <defect>",
    noBranch: "detour refused: it takes a workstream branch and a main branch (main or master)",
    exists: "detour refused: {0} or {1} already exists — pick another name",
    stashFailed: "detour refused: the work in progress cannot be set aside (unresolved conflict?) — nothing changed",
    switchFailed: "detour refused: the branch cannot be created ({0}) — nothing changed",
    opened: "detour opened: branch {0} from {1}, brief {2}{3}.",
    stashed: ", work in progress set aside (stash {0})",
    steps: [
      "Next, in order:",
      "  1. Scope the brief {2} (scope block: the fix and its test, nothing else) and commit it alone.",
      "  2. Test first, seen failing; then the fix (« fix a defect » recipe); `node .githooks/run-checks.mjs`.",
      `  3. Journal entry (${JOURNAL}) « Detour of {0}: … » citing branch {1}, then delete the brief; commit.`,
      "  4. Push, PR titled « Detour of {0}: … », {3}.",
      "  5. PR merged: `node .githooks/detour.mjs resume`.",
    ].join("\n"),
    auto: "automatic merge scheduled (`gh pr merge --auto --merge`)",
    manual: "merge left to the human",
    rulesAhead: "⚠ these files change the rules: the human's yes will be asked (`allow`) — {0}",
    noDetour: "no detour in progress",
    dirty: "resume refused: worktree modified — commit or remove first (nothing was touched)",
    notMerged: "resume refused: {0} is not merged into {1} yet — wait for its PR to be merged",
    briefLeft: "resume refused: brief {0} is still in {1} — the detour's PR deletes it",
    noJournal: `resume refused: no entry of ${JOURNAL} cites {0} in {1} — the detour's PR adds it`,
    switchBack: "resume refused: cannot go back to {0} — state unchanged, set-aside work intact",
    rebase:
      "resume refused: {0} does not rebase on {1} without conflict (rebase aborted) — state unchanged, set-aside work intact; a human decides",
    stashMissing: "resume refused: the set-aside work {0} is not in `git stash list` — state unchanged",
    restore: "resume refused: restoring the work conflicts — worktree cleaned, set-aside work {0} intact, state unchanged; a human decides",
    resumed: "detour closed: back on {1}, rebased on {2}, work restored; BUILD on {0}.",
    forcePush: "The branch was already pushed: `git push --force-with-lease`.",
    human: "the human's yes is given in an interactive terminal (`node .githooks/detour.mjs allow`); in a queue, the subagent stops",
    confirm: "Detour {0} changes the rules. Allow it? Type DETOUR: ",
    refused: "yes not given: nothing changed",
    allowed: "detour {0} allowed to change the rules",
    brief: `# Detour: {0} (detour of {1})

**Status**: opened on {3} (detour of {1}, branch {2}).
**Risk**: {4}
**Maturity**: in progress

<!-- scope
files:
{5}
-->

(scope: the fix and its test, nothing else; the scope of {1} never widens for a detour.)

## 1. Need

{0}: prior defect met during {1}, outside its scope.

## Acceptance criteria

- {0}: fixed, kept by a non-regression test seen failing before the fix

## 4. Decisions

- A detour opens without asking: the check is at its PR's merge. Related contracts: QUA-017 (no direct work on the main branch), QUA-019 (no bypassing a check).

## 7. Hand-off

- PR titled « Detour of {1}: {0} »; before the push, a journal entry citing branch {2} and this brief deleted; PR merged: \`node .githooks/detour.mjs resume\`.
`,
  },
};

const texts = (root) => TEXTS[config(root).lang === "en" ? "en" : "fr"];
const done = (message) => ({ code: 0, message });
const refused = (message) => ({ code: 1, message });

const git = (root, ...args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout.trim() : null;
};

/**
 * Files of `files` that change the rules (contracts registry, hooks, thresholds, CI), the template
 * sources of the project's declared copies (`scopeMirrors`) included.
 */
export function rulesFiles(files, cfg = {}) {
  const patterns = [registryPath(cfg), ...RULES, ...(Array.isArray(cfg.ciFiles) ? cfg.ciFiles : [])];
  return files.map(normalize).filter((f) => targets(f, cfg).some((p) => patterns.some((g) => fnmatch(p, g))));
}

const registryPath = (cfg) => cfg.dirs?.contracts ?? "docs/contracts.md";

/** `f` and the copies it is the template source of (`scopeMirrors`). */
function targets(f, cfg) {
  const mirrors = (Array.isArray(cfg.scopeMirrors) ? cfg.scopeMirrors : []).filter(
    (m) => typeof m?.from === "string" && m.from && typeof m.to === "string",
  );
  return [f, ...mirrors.filter((m) => f.startsWith(m.from)).map((m) => m.to + f.slice(m.from.length))];
}

/** True if `f` is the contracts registry, or the template source of its copy. */
export function isRegistry(f, cfg = {}) {
  return targets(normalize(f), cfg).some((p) => fnmatch(p, registryPath(cfg)));
}

const HEADING = /^##\s+([A-Z][A-Z0-9]*-\d+)\s*(?:[—–-]\s*.*)?$/;

/** Blocks of a registry keyed by their `## ` heading (the preamble under ""); null if a heading repeats. */
function blocks(text) {
  const map = new Map();
  let key = "";
  let lines = [];
  const flush = () => !map.has(key) && map.set(key, lines.join("\n").trimEnd());
  for (const line of text.split(/\r?\n/)) {
    if (!line.startsWith("## ")) lines.push(line);
    else if (!flush()) return null;
    else [key, lines] = [line, []];
  }
  return flush() ? map : null;
}

/**
 * QUA-050: true if `after` only adds new contracts to `before` — every existing block (preamble
 * included) kept word for word, each new block a contract heading whose ID `before` never defines.
 * In doubt (a registry missing, a repeated heading), false: the human's yes stays asked.
 */
export function contractsAddedOnly(before, after) {
  if (typeof before !== "string" || typeof after !== "string") return false;
  const old = blocks(before);
  const next = blocks(after);
  if (!old || !next) return false;
  if ([...old].some(([k, v]) => next.get(k) !== v)) return false;
  const known = (id) => new RegExp(`^(?:##\\s+|\\|\\s*)${id}\\b`, "m").test(before);
  return [...next.keys()].filter((k) => !old.has(k)).every((k) => HEADING.test(k) && !known(HEADING.exec(k)[1]));
}

/** The ref a detour starts from and a resume rebases on: the remote main branch, fetched, if any; else the local one. */
function baseRef(root, main) {
  if (git(root, "rev-parse", "--verify", "--quiet", `refs/remotes/origin/${main}`) === null) return main;
  git(root, "fetch", "-q", "origin", main);
  return `origin/${main}`;
}

/** Opens a detour (see the header). `{ code, message }`. */
export function open(root, name, { title, files = [], now = new Date().toISOString() } = {}) {
  const T = texts(root);
  const cfg = config(root);
  const { state } = readState(root);
  if (state.activity !== "BUILD") return refused(fill(T.notBuild, state.activity));
  if (state.detour) return refused(fill(T.already, state.detour.branch));
  if (!/^[a-z0-9][a-z0-9-]*$/.test(name ?? "") || !title) return refused(T.name);
  const main = mainBranch(root);
  const current = git(root, "symbolic-ref", "--quiet", "--short", "HEAD");
  if (!main || !current || current === main) return refused(T.noBranch);
  const branch = `detour/${name}`;
  const brief = `${BRIEF_DIRS[0]}detour-${name}.md`;
  if (git(root, "rev-parse", "--verify", "--quiet", `refs/heads/${branch}`) !== null || existsSync(join(root, brief)))
    return refused(fill(T.exists, branch, brief));
  const base = baseRef(root, main);
  // The branch first: a failed `git switch -c` leaves the worktree half switched; `git branch` touches nothing.
  const r = spawnSync("git", ["branch", branch, base], { cwd: root, encoding: "utf8" });
  if (r.status !== 0) return refused(fill(T.switchFailed, r.stderr.trim()));
  let stash = null;
  if (git(root, "status", "--porcelain") !== "") {
    if (git(root, "stash", "push", "-q", "--include-untracked", "-m", `drwil-detour: ${name}`) === null) {
      git(root, "branch", "-D", branch);
      return refused(T.stashFailed);
    }
    stash = git(root, "rev-parse", "stash@{0}");
  }
  // Clean worktree (everything set aside), new branch from an existing commit: nothing left to block it.
  git(root, "switch", "-q", branch);
  const list = files.map(normalize);
  const risk = minimumLevel(list, cfg.risk?.sensitivePaths ?? SENSITIVE_PATHS) === "HIGH" ? "HIGH" : "MEDIUM";
  mkdirSync(dirname(join(root, brief)), { recursive: true });
  const origin = state.active_brief;
  const scopeLines = list.map((f) => `  - ${f}`).join("\n");
  writeFileSync(join(root, brief), fill(T.brief, title, origin, branch, now.slice(0, 10), risk, scopeLines));
  setDetour(root, brief, { origin_brief: origin, origin_branch: current, branch, stash }, now);
  const rules = rulesFiles(list, cfg);
  const report = [
    fill(T.opened, branch, base, brief, stash ? fill(T.stashed, stash.slice(0, 7)) : ""),
    fill(T.steps, origin, branch, brief, state.auto_merge === true ? T.auto : T.manual),
  ];
  if (rules.length) report.push(fill(T.rulesAhead, rules.join(", ")));
  return done(report.join("\n"));
}

/** Closes the detour and goes back to the workstream (see the header). `{ code, message }`. */
export function resume(root, { now = new Date().toISOString(), gh } = {}) {
  const T = texts(root);
  const { state } = readState(root);
  const d = state.detour;
  if (!d) return refused(T.noDetour);
  if (git(root, "status", "--porcelain") !== "") return refused(T.dirty);
  const base = baseRef(root, mainBranch(root) ?? "main");
  // A PR merged by squash leaves no trace git can see: gh proves it (QUA-058), git alone without it (QUA-059).
  let merged = branchMerged(root, d.branch, base, []);
  if (merged === "no") merged = branchMerged(root, d.branch, base, mergedPullRequests(root, { branch: d.branch, gh }));
  if (merged === "unknown") return refused(`${fill(T.notMerged, d.branch, base)} (${noGhText(root)})`);
  if (merged === "no") return refused(fill(T.notMerged, d.branch, base));
  if (git(root, "cat-file", "-e", `${base}:${state.active_brief}`) !== null) return refused(fill(T.briefLeft, state.active_brief, base));
  if (!(git(root, "show", `${base}:${JOURNAL}`) ?? "").includes(d.branch)) return refused(fill(T.noJournal, d.branch, base));
  if (git(root, "switch", "-q", d.origin_branch) === null) return refused(fill(T.switchBack, d.origin_branch));
  if (git(root, "rebase", "-q", base) === null) {
    git(root, "rebase", "--abort");
    return refused(fill(T.rebase, d.origin_branch, base));
  }
  if (d.stash) {
    const n = git(root, "stash", "list", "--format=%H").split("\n").indexOf(d.stash);
    if (n < 0) return refused(fill(T.stashMissing, d.stash.slice(0, 7)));
    if (git(root, "stash", "apply", "-q", `stash@{${n}}`) === null) {
      // The worktree was clean before the apply: what is there now comes from the stash, which stays.
      git(root, "reset", "-q", "--hard");
      git(root, "clean", "-fdq");
      return refused(fill(T.restore, d.stash.slice(0, 7)));
    }
    git(root, "stash", "drop", "-q", `stash@{${n}}`);
  }
  setDetour(root, d.origin_brief, null, now);
  const pushed = git(root, "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}") !== null;
  return done([fill(T.resumed, d.origin_brief, d.origin_branch, base), ...(pushed ? [T.forcePush] : [])].join("\n"));
}

/** The human's yes for a detour that changes the rules; `confirm(question)` only in an interactive terminal. */
export async function allow(root, { tty = false, confirm } = {}) {
  const T = texts(root);
  const { state } = readState(root);
  if (!state.detour) return refused(T.noDetour);
  if (!tty || typeof confirm !== "function") return refused(T.human);
  if (!(await confirm(fill(T.confirm, state.detour.branch)))) return refused(T.refused);
  setDetour(root, state.active_brief, { ...state.detour, human: true });
  return done(fill(T.allowed, state.detour.branch));
}

/** Value of the `--name value` option in `args`, or undefined. */
function option(args, name) {
  const i = args.indexOf(name);
  return i >= 0 && i + 1 < args.length ? args[i + 1] : undefined;
}

/** Entry point: `node .githooks/detour.mjs open|resume|allow …`. Returns the exit code. */
export async function main(args, root) {
  let r;
  if (args[0] === "open") {
    const files = (option(args, "--files") ?? "").split(",").filter(Boolean);
    r = open(root, args[1], { title: option(args, "--title"), files });
  } else if (args[0] === "resume") r = resume(root);
  else if (args[0] === "allow") {
    const tty = Boolean(process.stdin.isTTY && process.stdout.isTTY);
    const confirm = async (question) => {
      const { createInterface } = await import("node:readline/promises");
      const rl = createInterface({ input: process.stdin, output: process.stdout });
      try {
        return (await rl.question(question)).trim() === "DETOUR";
      } finally {
        rl.close();
      }
    };
    r = await allow(root, { tty, confirm });
  } else {
    console.error(texts(root).usage);
    return 2;
  }
  (r.code === 0 ? console.log : console.error)(`${r.code === 0 ? "✓" : "✗"} ${r.message}`);
  return r.code;
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = await main(process.argv.slice(2), stateRoot());
}
