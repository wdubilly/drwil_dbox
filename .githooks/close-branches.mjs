// Merged local branches: recognised (classic merge, or pull request merged by squash through gh,
// QUA-058; git alone without gh, QUA-059) and deleted when the next workstream starts
// (`pruneMergedBranches`, called by `start` in state.mjs). `branchMerged` is shared with the
// detour's resume. The closing command (close.mjs) re-exports these functions, QUA-048.
import { spawnSync } from "node:child_process";
import { config } from "./briefs.mjs";

const TEXTS = {
  fr: {
    pruned: (l) => `branches locales fusionnées supprimées : ${l}`,
    noGh: "gh injoignable : seule la fusion classique est reconnue, une PR fusionnée par squash ne l'est pas",
    kept: (l) => `branches gardées : ${l}`,
  },
  en: {
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
export const ghRun = (root, args) => {
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
