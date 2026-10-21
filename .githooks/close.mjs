#!/usr/bin/env node
// Closing of a workstream in one gesture (QUA-048): `node .githooks/close.mjs` (in CLOSING) deletes
// the brief, removes its index line and records the consumption; `--check` (any activity) only
// reports, before the commit, what would make the closing fail. It never stages, commits, pushes nor
// merges anything, and it never writes the state: the journal entry and the limits stay the agent's.
// The local branches already merged are deleted when the next workstream starts (`pruneMergedBranches`,
// called by `start` in state.mjs); `branchMerged`, shared with the detour's resume, also recognises a
// pull request merged by squash, through gh (QUA-058), and falls back to git without it (QUA-059).
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, realpathSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, posix } from "node:path";
import { fileURLToPath } from "node:url";
import { acceptanceCriteria, config } from "./briefs.mjs";
import { changedFiles } from "./check-code-rules.mjs";
import { main as consumption } from "./consumption.mjs";
import { definedIds } from "./contracts.mjs";
import { readState, stateRoot, verifyEvidence } from "./state.mjs";

const TEXTS = {
  fr: {
    usage: "usage : node .githooks/close.mjs [--check] [<chemin de la fiche>] [--logs <dossier>]",
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
    clean: "Rien ne ferait échouer la clôture.",
    rest: "Reste à l'agent : l'entrée du journal (décisions, limites en intentions), puis le commit de clôture.",
    pruned: (l) => `branches locales fusionnées supprimées : ${l}`,
    noGh: "gh injoignable : seule la fusion classique est reconnue, une PR fusionnée par squash ne l'est pas",
    kept: (l) => `branches gardées : ${l}`,
  },
  en: {
    usage: "usage: node .githooks/close.mjs [--check] [<brief path>] [--logs <folder>]",
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
    clean: "Nothing would make the closing fail.",
    rest: "Left to the agent: the journal entry (decisions, limits as intents), then the closing commit.",
    pruned: (l) => `merged local branches deleted: ${l}`,
    noGh: "gh unreachable: classic merge only is recognised, a pull request merged by squash is not",
    kept: (l) => `branches kept: ${l}`,
  },
};

const texts = (root) => TEXTS[config(root).lang === "en" ? "en" : "fr"];

const git = (root, ...args) => {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout.trim() : null;
};

/** The local main branch (`main` or `master`), or null. */
export function mainBranch(root) {
  return ["main", "master"].find((b) => git(root, "rev-parse", "--verify", "--quiet", `refs/heads/${b}`) !== null) ?? null;
}

// gh absent, unauthenticated, outside GitHub or too slow: null, and the caller falls back to git.
const ghRun = (root, args) => {
  const r = spawnSync("gh", args, { cwd: root, encoding: "utf8", timeout: 20000 });
  return r.status === 0 ? r.stdout : null;
};

/** The merged pull requests (`{ headRefName, headRefOid }`, of `branch` alone if given); null if gh does not answer. */
export function mergedPullRequests(root, { branch, gh = ghRun } = {}) {
  const args = ["pr", "list", "--state", "merged", "--json", "headRefName,headRefOid", "--limit", branch ? "20" : "200"];
  try {
    const prs = JSON.parse(gh(root, branch ? [...args, "--head", branch] : args) ?? "");
    return Array.isArray(prs) ? prs : null;
  } catch {
    return null;
  }
}

/**
 * Is `branch` merged into `main`? "git" if git sees it (classic merge); "gh" if a merged pull request
 * of this branch carries its tip (squash included: work committed after the PR is never "merged");
 * otherwise "no" when gh answered (`prs`, from `mergedPullRequests`), "unknown" when it did not (null).
 */
export function branchMerged(root, branch, main, prs) {
  if (git(root, "merge-base", "--is-ancestor", branch, main) !== null) return "git";
  if (prs === null) return "unknown";
  const tip = git(root, "rev-parse", "--verify", "--quiet", `refs/heads/${branch}`);
  const carried = (pr) =>
    pr.headRefName === branch && (pr.headRefOid === tip || git(root, "merge-base", "--is-ancestor", tip, pr.headRefOid) !== null);
  return tip !== null && prs.some(carried) ? "gh" : "no";
}

/**
 * Is the closing pull request merged? The state does not keep the workstream's branch: on the main
 * branch (updated after the merge) the closing commit is there; on another branch, `branchMerged`
 * answers for it. "git" | "gh" | "no" | "unknown" (gh silent, or no main branch / detached HEAD).
 */
export function closingMerged(root, { gh } = {}) {
  const main = mainBranch(root);
  const current = git(root, "symbolic-ref", "--quiet", "--short", "HEAD");
  if (!main || !current) return "unknown";
  if (current === main) return "git";
  const verdict = branchMerged(root, current, main, []);
  return verdict === "no" ? branchMerged(root, current, main, mergedPullRequests(root, { branch: current, gh })) : verdict;
}

/**
 * Deletes the local branches proved merged into the main branch (`branchMerged`), never the main
 * branch, the current one, nor a branch still at the main branch's tip (just created, its work not
 * started). `{ deleted, unproven }`: unproven, the branches kept because gh did not answer; outside git:
 * none. One gh question for all the branches, asked only if git does not see them all merged.
 */
export function pruneMergedBranches(root, { gh } = {}) {
  const main = mainBranch(root);
  if (!main) return { deleted: [], unproven: [] };
  const current = git(root, "symbolic-ref", "--quiet", "--short", "HEAD");
  const tip = git(root, "rev-parse", main);
  const branches = git(root, "branch", "--format=%(refname:short)").split("\n");
  const candidates = branches.filter((b) => b !== main && b !== current && git(root, "rev-parse", b) !== tip);
  const byGit = candidates.map((b) => [b, branchMerged(root, b, main, [])]);
  const prs = byGit.some(([, v]) => v === "no") ? mergedPullRequests(root, { gh }) : [];
  const verdicts = byGit.map(([b, v]) => [b, v === "no" ? branchMerged(root, b, main, prs) : v]);
  // `-D` only on gh's proof: git, which does not see a squash, would refuse `-d`.
  const deleted = verdicts.filter(([b, v]) => (v === "git" || v === "gh") && git(root, "branch", v === "gh" ? "-D" : "-d", b) !== null);
  return { deleted: deleted.map(([b]) => b), unproven: verdicts.filter(([, v]) => v === "unknown").map(([b]) => b) };
}

/** Message of `start` for the branches it deleted, and those kept for lack of gh ("" if none). */
export function prunedMessage(root, { deleted, unproven }) {
  const T = texts(root);
  return (
    (deleted.length ? `\n${T.pruned(deleted.join(", "))}` : "") + (unproven.length ? `\n${T.noGh} — ${T.kept(unproven.join(", "))}` : "")
  );
}

/** Text saying gh did not answer (for the detour's resume refusal). */
export const noGhText = (root) => texts(root).noGh;

const indexPath = (root) => config(root).dirs?.index ?? "docs/projects/pending.md";

/** Lines [from, to) of the index item citing this brief, and it alone; null if there is none. */
function indexItem(lines, brief) {
  for (let i = 0; i < lines.length; i++) {
    if (!/^\s*- \[[ xX]\]/.test(lines[i])) continue;
    let end = i + 1;
    while (end < lines.length && /^\s+\S/.test(lines[end]) && !/^\s*- \[/.test(lines[end])) end++;
    const cited = new Set(
      lines
        .slice(i, end)
        .join("\n")
        .match(/`docs\/[^`]+\.md`/g),
    );
    if (cited.has(`\`${brief}\``) && cited.size === 1) return [i, end];
  }
  return null;
}

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

/** Places (file:line) citing the brief outside the brief itself and its index item. */
function deadReferences(root, brief) {
  const T = texts(root);
  const out = git(root, "grep", "-n", "-I", "--untracked", "-F", brief) ?? "";
  const index = indexPath(root);
  const item = existsSync(join(root, index)) ? indexItem(readFileSync(join(root, index), "utf8").split(/\r?\n/), brief) : null;
  return out
    .split("\n")
    .filter(Boolean)
    .map((l) => /^(.+?):(\d+):/.exec(l))
    .filter(([, f, n]) => f !== brief && !(f === index && item && Number(n) > item[0] && Number(n) <= item[1]))
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

/** What would make the closing of `brief` fail, as messages; `text`: the brief, read before it is deleted. */
export function closingSignals(root, brief, text = existsSync(join(root, brief)) ? readFileSync(join(root, brief), "utf8") : "") {
  return [...criteriaToCreate(root, brief, text), ...deadReferences(root, brief), ...bareNames(root)];
}

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
 * removed, consumption recorded — then the signals. With `check`: the signals only.
 */
export function close(root, { brief, check = false, env = process.env, logs } = {}) {
  const T = texts(root);
  const { state } = readState(root);
  const path = brief ? posix.normalize(brief.replace(/\\/g, "/")) : state.active_brief;
  if (!path) return { code: 1, message: T.noBrief };
  const route = pathToClosing(root, state.activity);
  if (!check && state.activity !== "CLOSING") return { code: 1, message: [T.notClosing(state.activity), route].filter(Boolean).join("\n") };
  const text = existsSync(join(root, path)) ? readFileSync(join(root, path), "utf8") : "";
  const done = [];
  if (!check) {
    if (existsSync(join(root, path))) {
      unlinkSync(join(root, path));
      done.push(T.deleted(path));
    }
    const index = indexPath(root);
    const lines = existsSync(join(root, index)) ? readFileSync(join(root, index), "utf8").split("\n") : [];
    const item = indexItem(lines, path);
    if (item) {
      lines.splice(item[0], item[1] - item[0]);
      writeFileSync(join(root, index), lines.join("\n"));
      done.push(T.indexLine(index));
    } else done.push(T.noIndexLine(index, path));
    const record = ["--record", posix.basename(path, ".md"), ...(logs ? ["--logs", logs] : [])];
    consumption(record, { env, root, log: (m) => done.push(m), err: (m) => done.push(m) });
  }
  const signals = closingSignals(root, path, text);
  const report = signals.length ? [T.signals, ...signals.map((s) => `  ⚠ ${s}`)] : [T.clean];
  return { code: signals.length ? 1 : 0, message: [...done, ...report, ...(check ? [route] : [T.rest])].filter(Boolean).join("\n") };
}

/** Entry point: `node .githooks/close.mjs [--check] [<brief>] [--logs <folder>]`. Returns the exit code. */
export function main(args, root, { env = process.env } = {}) {
  const i = args.indexOf("--logs");
  const logs = i >= 0 ? args[i + 1] : undefined;
  const rest = args.filter((a, j) => a !== "--check" && (i < 0 || (j !== i && j !== i + 1)));
  if (rest.some((a) => a.startsWith("-")) || rest.length > 1) {
    console.error(texts(root).usage);
    return 2;
  }
  const r = close(root, { brief: rest[0], check: args.includes("--check"), env, logs });
  (r.code === 0 ? console.log : console.error)(r.message);
  return r.code;
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = main(process.argv.slice(2), stateRoot());
}
