// One workstream per worktree (QUA-142 to QUA-146): the worktrees of the repository, the launch of a
// workstream in a new sibling folder, the guard against the same brief active in two folders, the
// reminder that tells an agent the folders are one repository, and the lag of a workstream branch
// behind the main branch. Standalone: `state.mjs` calls it, it reads the other folders' state files
// itself and imports `state.mjs` only at launch time (no import cycle at load).
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, realpathSync } from "node:fs";
import { basename, dirname, join, posix } from "node:path";
import { config, queueable, scopedBriefs, scopesOverlap } from "./briefs.mjs";
import { mainBranch } from "./close-branches.mjs";

const DEFAULT_MAX_PARALLEL = 3;

const TEXTS = {
  fr: {
    inWorktree:
      "Ce dossier est un worktree du dépôt principal {0} (branche {1}, chantier {2}). Les autres dossiers listés ci-dessous sont des worktrees du même dépôt, non des dépôts distincts : ne modifier que celui-ci.",
    inMain:
      "Ce dossier est le dépôt principal ; les dossiers listés ci-dessous sont des worktrees ouverts du même dépôt, non des dépôts distincts : n'y modifier aucun fichier depuis cette session.",
    entry: "  - {0} — branche {1}, chantier {2}",
    none: "aucun",
    noRepo: "worktree refusé : ce dossier n'est pas un dépôt git lisible ; rien n'est créé",
    manyBriefs: "worktree refusé : un seul chantier à la fois par worktree (la file reste dans le dossier du dépôt) ; rien n'est créé",
    parallelOne: "file en parallèle refusée : au moins deux fiches sont attendues ; rien n'est créé",
    parallelDuplicate: "file en parallèle refusée : la fiche {0} est choisie deux fois ; rien n'est créé",
    parallelNotReady:
      "file en parallèle refusée : {0} ne peut pas entrer dans une file (cadrée, maturité « prêt », aucune décision ouverte, critères d'acceptation) ; rien n'est créé",
    parallelTooMany:
      "file en parallèle refusée : {0} chantier(s) déjà actifs, {1} demandés, au plus {2} à la fois (réglage « maxParallel » de .drwil/ia-first.json) ; rien n'est créé",
    parallelOverlap:
      "file en parallèle refusée : les cadrages de {0} et de {1} se recoupent ({2} et {3}), ils se rencontreraient à la fusion ; rien n'est créé",
    parallelDone: "File en parallèle : {0} chantier(s) lancé(s) sur {1}, chacun dans son worktree, au plus {2} actifs à la fois.",
    parallelStopped: "✗ File en parallèle interrompue à {0} : les suivants ne sont pas lancés, ceux déjà lancés tournent.",
    notScoped: "worktree refusé : {0} n'est pas une fiche cadrée ; rien n'est créé",
    busy: "worktree refusé : la fiche {0} est déjà active dans {1} ; rien n'est créé",
    noMain: "worktree refusé : aucune branche principale locale (main ou master) ; rien n'est créé",
    exists: "worktree refusé : {0} existe déjà ; rien n'est créé",
    branchExists: "worktree refusé : la branche {0} existe déjà ; rien n'est créé",
    stale: "git fetch a échoué : le worktree part de la branche locale {0}, peut-être en retard sur la branche principale distante",
    addFailed: "worktree refusé : git worktree add a échoué ({0}) ; rien n'est créé",
    created: "Worktree créé : {0} (branche {1}, depuis {2}).",
    noInstall:
      "Aucune installation lancée (réglage « worktreeInstall » absent de .drwil/ia-first.json) : installer les dépendances du projet dans ce dossier avant de travailler.",
    installed: "Installation lancée dans le worktree : {0} (réussie).",
    installFailed:
      "✗ Installation en échec dans le worktree : {0} ({1}) ; le worktree et l'état restent, relancer la commande à la main dans ce dossier.",
    startFailed: "✗ Chantier non lancé dans le worktree : {0} ; le worktree reste, l'état du dossier d'origine est inchangé.",
    open: "Ouvrir un terminal dans {0} et y lancer l'agent : le chantier se mène là. Le dossier d'origine reste inchangé : ne pas y travailler cette fiche.",
    late: "branche en retard de {0} commit(s) sur origin/{1} : `git merge origin/{1}` avant de clore (la clôture n'est pas refusée pour ce motif)",
    lateUnknown: "retard sur origin/{0} non mesuré (git fetch a échoué, réseau ?) : rien n'est conclu",
  },
  en: {
    inWorktree:
      "This folder is a worktree of the main repository {0} (branch {1}, workstream {2}). The other folders listed below are worktrees of the same repository, not separate repositories: only modify this one.",
    inMain:
      "This folder is the main repository; the folders listed below are open worktrees of the same repository, not separate repositories: modify none of their files from this session.",
    entry: "  - {0} — branch {1}, workstream {2}",
    none: "none",
    noRepo: "worktree refused: this folder is not a readable git repository; nothing created",
    manyBriefs: "worktree refused: one workstream at a time per worktree (a queue stays in the repository folder); nothing created",
    parallelOne: "parallel queue refused: at least two briefs are expected; nothing created",
    parallelDuplicate: "parallel queue refused: brief {0} is chosen twice; nothing created",
    parallelNotReady:
      'parallel queue refused: {0} cannot enter a queue (scoped, maturity "ready", no open decision, acceptance criteria); nothing created',
    parallelTooMany:
      'parallel queue refused: {0} workstream(s) already active, {1} requested, at most {2} at a time ("maxParallel" setting of .drwil/ia-first.json); nothing created',
    parallelOverlap:
      "parallel queue refused: the scopes of {0} and {1} overlap ({2} and {3}), they would meet at the merge; nothing created",
    parallelDone: "Parallel queue: {0} workstream(s) launched out of {1}, each in its own worktree, at most {2} active at a time.",
    parallelStopped: "✗ Parallel queue interrupted at {0}: the next ones are not launched, those already launched keep running.",
    notScoped: "worktree refused: {0} is not a scoped brief; nothing created",
    busy: "worktree refused: brief {0} is already active in {1}; nothing created",
    noMain: "worktree refused: no local main branch (main or master); nothing created",
    exists: "worktree refused: {0} already exists; nothing created",
    branchExists: "worktree refused: branch {0} already exists; nothing created",
    stale: "git fetch failed: the worktree starts from the local branch {0}, possibly behind the remote main branch",
    addFailed: "worktree refused: git worktree add failed ({0}); nothing created",
    created: "Worktree created: {0} (branch {1}, from {2}).",
    noInstall:
      'No installation run (no "worktreeInstall" setting in .drwil/ia-first.json): install the project dependencies in this folder before working.',
    installed: "Installation run in the worktree: {0} (succeeded).",
    installFailed:
      "✗ Installation failed in the worktree: {0} ({1}); the worktree and the state stay, run the command by hand in that folder.",
    startFailed: "✗ Workstream not started in the worktree: {0}; the worktree stays, the original folder's state is unchanged.",
    open: "Open a terminal in {0} and start the agent there: the workstream runs there. The original folder stays unchanged: do not work this brief in it.",
    late: "branch {0} commit(s) behind origin/{1}: `git merge origin/{1}` before closing (the closing is not refused for this reason)",
    lateUnknown: "lag behind origin/{0} not measured (git fetch failed, network?): nothing concluded",
  },
};

const label = (value, fallback) => value || fallback;
const fill = (message, ...values) => message.replace(/\{(\d)\}/g, (_, i) => values[i]);
const texts = (root) => TEXTS[config(root).lang === "en" ? "en" : "fr"];

const git = (root, ...args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout.trim() : null;
};

/** The worktrees (`{ path, branch }`, the first one is the main repository); null if git does not answer. */
export function worktrees(root) {
  const out = git(root, "worktree", "list", "--porcelain");
  if (out === null) return null;
  return out
    .split(/\n\n+/)
    .map((block) => ({
      path: /^worktree (.+)$/m.exec(block)?.[1],
      branch: /^branch refs\/heads\/(.+)$/m.exec(block)?.[1],
    }))
    .filter((w) => w.path);
}

// The brief another folder works on, read from its own state file: absent, unreadable or SCOPING = none.
function briefOf(dir) {
  try {
    const s = JSON.parse(readFileSync(join(dir, ".drwil", "state.json"), "utf8"));
    return s.activity !== "SCOPING" && typeof s.active_brief === "string" ? s.active_brief : null;
  } catch {
    return null;
  }
}

/** `{ self, main, others }` for `root`, which must be a worktree root; a folder listed but gone is ignored; null otherwise. */
export function situation(root) {
  const all = worktrees(root)?.filter((w) => existsSync(w.path));
  if (!all?.length) return null;
  // `.native`: on Windows git answers the long name while a temporary folder may carry the short (8.3) one.
  const me = realpathSync.native(root);
  const self = all.find((w) => realpathSync.native(w.path) === me);
  if (!self) return null;
  const others = all.filter((w) => w !== self).map((w) => ({ ...w, brief: briefOf(w.path) }));
  return { self: { ...self, brief: briefOf(self.path) }, main: all[0], others };
}

/** The folder, other than `root`, where `brief` is active; null if none (QUA-145). */
export function activeElsewhere(root, brief) {
  return situation(root)?.others.find((w) => w.brief === brief)?.path ?? null;
}

/** Lines of the state reminder about the worktrees (QUA-143); empty outside any worktree arrangement. */
export function worktreeLines(root) {
  const s = situation(root);
  if (!s || (s.self.path === s.main.path && !s.others.length)) return [];
  const T = texts(root);
  const inWorktree = s.self.path !== s.main.path;
  const head = inWorktree ? fill(T.inWorktree, s.main.path, label(s.self.branch, "?"), label(s.self.brief, T.none)) : T.inMain;
  return [head, ...s.others.map((w) => fill(T.entry, w.path, label(w.branch, "?"), label(w.brief, T.none)))];
}

function install(root, dir) {
  const T = texts(root);
  const command = config(root).worktreeInstall;
  if (typeof command !== "string" || !command.trim()) return T.noInstall;
  const r = spawnSync(command, {
    cwd: dir,
    shell: true,
    encoding: "utf8",
    timeout: 600000,
  });
  if (r.status === 0) return fill(T.installed, command);
  return fill(T.installFailed, command, `${r.stderr}${r.stdout}`.trim().split("\n").slice(-3).join(" ⏎ "));
}

/**
 * Launches `brief` in a new sibling worktree `../<repository>-<brief>` (QUA-142), branch `workstream/<brief>`
 * cut from the up-to-date main branch, never touching the repository's configuration (`--no-track`, QUA-056).
 * Only for a human's answer: the state of the new folder is written by `start` of state.mjs, the origin
 * folder's state does not change. `{ code, message, path? }`.
 */
export async function launchInWorktree(root, brief, { autoMerge = false, queue = [], now } = {}) {
  const T = texts(root);
  const s = situation(root);
  if (!s) return { code: 1, message: T.noRepo };
  if (queue.length) return { code: 1, message: T.manyBriefs };
  const path = posix.normalize(String(brief).trim().replace(/\\/g, "/"));
  if (!scopedBriefs(root).some((b) => b.path === path)) return { code: 1, message: fill(T.notScoped, brief) };
  const holder = [s.self, ...s.others].find((w) => w.brief === path);
  if (holder) return { code: 1, message: fill(T.busy, path, holder.path) };
  const main = mainBranch(root);
  if (!main) return { code: 1, message: T.noMain };
  const name = basename(path, ".md");
  const dir = join(dirname(s.main.path), `${basename(s.main.path)}-${name}`);
  const branch = `workstream/${name}`;
  if (existsSync(dir)) return { code: 1, message: fill(T.exists, dir) };
  if (git(root, "rev-parse", "--verify", "--quiet", `refs/heads/${branch}`) !== null)
    return { code: 1, message: fill(T.branchExists, branch) };
  const fetched = spawnSync("git", ["fetch", "--quiet", "origin", main], {
    cwd: root,
    encoding: "utf8",
    timeout: 20000,
    env: { ...process.env, GIT_TERMINAL_PROMPT: "0" },
  });
  const base = fetched.status === 0 ? `origin/${main}` : main;
  const added = spawnSync("git", ["worktree", "add", "--no-track", "-b", branch, dir, base], { cwd: root, encoding: "utf8" });
  if (added.status !== 0) return { code: 1, message: fill(T.addFailed, added.stderr.trim()) };
  const lines = [fill(T.created, dir, branch, base)];
  if (fetched.status !== 0) lines.push(fill(T.stale, main));
  lines.push(install(root, dir));
  const { start } = await import("./state.mjs");
  const started = start(dir, path, { human: true, autoMerge, now });
  lines.push(started.code === 0 ? started.message : fill(T.startFailed, started.message), fill(T.open, dir));
  return { code: started.code, message: lines.join("\n"), path: dir };
}

const maxParallel = (root) => {
  const n = config(root).maxParallel;
  return Number.isInteger(n) && n > 0 ? n : DEFAULT_MAX_PARALLEL;
};

/**
 * Launches a queue in parallel (QUA-147), the human's answer being the only way in: each brief in its
 * own worktree by `launchInWorktree`, at most `maxParallel` workstreams active at once (default 3),
 * briefs whose scopes overlap refused. Everything is checked before anything is created; a launch that
 * fails stops the next ones, never those already launched. The launched briefs are recorded in this
 * folder's state (`parallel`). `{ code, message }`.
 */
export async function launchParallel(root, briefs, { autoMerge = false, now } = {}) {
  const T = texts(root);
  const s = situation(root);
  if (!s) return { code: 1, message: T.noRepo };
  const paths = briefs.map((b) => posix.normalize(String(b).trim().replace(/\\/g, "/")));
  if (paths.length < 2) return { code: 1, message: T.parallelOne };
  const twice = paths.find((b, i) => paths.indexOf(b) !== i);
  if (twice) return { code: 1, message: fill(T.parallelDuplicate, twice) };
  const notReady = paths.find((b) => !queueable(root, b));
  if (notReady) return { code: 1, message: fill(T.parallelNotReady, notReady) };
  const active = [s.self, ...s.others].filter((w) => w.brief);
  const holder = active.find((w) => paths.includes(w.brief));
  if (holder) return { code: 1, message: fill(T.busy, holder.brief, holder.path) };
  const max = maxParallel(root);
  if (active.length + paths.length > max) return { code: 1, message: fill(T.parallelTooMany, active.length, paths.length, max) };
  const all = [...paths, ...active.map((w) => w.brief)];
  for (const [i, a] of paths.entries())
    for (const b of all.slice(i + 1)) {
      const meet = scopesOverlap(root, a, b);
      if (meet) return { code: 1, message: fill(T.parallelOverlap, a, b, meet.a, meet.b) };
    }
  const lines = [];
  const launched = [];
  for (const brief of paths) {
    const r = await launchInWorktree(root, brief, { autoMerge, now });
    lines.push(r.message);
    if (r.code !== 0) {
      lines.push(fill(T.parallelStopped, brief));
      break;
    }
    launched.push(brief);
  }
  if (launched.length) {
    const { recordParallel } = await import("./state.mjs");
    recordParallel(root, launched);
    lines.push(fill(T.parallelDone, launched.length, paths.length, max));
  }
  return { code: launched.length === paths.length ? 0 : 1, message: lines.join("\n\n") };
}

/** Lines of `close --check` about the lag of the workstream branch behind the main branch (QUA-146); never a refusal. */
export function lateLines(root) {
  const T = texts(root);
  const main = mainBranch(root);
  const current = git(root, "rev-parse", "--abbrev-ref", "HEAD");
  if (!main || !current || current === "HEAD" || current === main) return [];
  if (git(root, "remote", "get-url", "origin") === null) return [];
  const fetched = spawnSync("git", ["fetch", "--quiet", "origin", main], {
    cwd: root,
    encoding: "utf8",
    timeout: 20000,
    env: { ...process.env, GIT_TERMINAL_PROMPT: "0" },
  });
  if (fetched.status !== 0) return [fill(T.lateUnknown, main)];
  const behind = Number(git(root, "rev-list", "--count", `HEAD..origin/${main}`));
  return behind > 0 ? [fill(T.late, behind, main)] : [];
}
