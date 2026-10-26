#!/usr/bin/env node
// Follows the CI of a pull request after a push or its creation (QUA-100): waits until its checks are
// over (`gh pr view --json statusCheckRollup`), then says « green », « merged » or « red » with the failing checks. A red CI is fixed, never
// bypassed (QUA-019). Without gh reachable, or past the time limit, the follow-up is « not run »,
// never « green » (QUA-013). Valid for every AI tool: a plain Node script on top of `gh`.
// Usage: node .githooks/pr-watch.mjs [<pr number>] [--timeout <minutes>]
// Exit code: 0 green or merged, 1 red or closed without merge, 2 not run.
import { spawnSync } from "node:child_process";
import { readFileSync, realpathSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const TEXTS = {
  fr: {
    notRun: "[pr-watch] suivi non exécuté : {0}",
    noGh: "gh injoignable ou non connecté (gh auth login), ou aucune PR pour cette branche",
    noChecks: "aucun contrôle de CI sur la PR #{0}",
    timeout: "délai de {0} min dépassé, contrôles encore en cours sur la PR #{1}",
    merged: "[pr-watch] PR #{0} fusionnée.",
    closed: "[pr-watch] PR #{0} fermée sans fusion.",
    green: "[pr-watch] PR #{0} : CI verte ({1} contrôle(s)).",
    red: "[pr-watch] PR #{0} : CI rouge — en échec : {1}. Corriger la cause, jamais contourner (QUA-019).",
    waiting: "[pr-watch] PR #{0} : {1} contrôle(s) en cours…",
  },
  en: {
    notRun: "[pr-watch] follow-up not run: {0}",
    noGh: "gh unreachable or not logged in (gh auth login), or no PR for this branch",
    noChecks: "no CI check on PR #{0}",
    timeout: "{0} min limit reached, checks still running on PR #{1}",
    merged: "[pr-watch] PR #{0} merged.",
    closed: "[pr-watch] PR #{0} closed without merge.",
    green: "[pr-watch] PR #{0}: CI green ({1} check(s)).",
    red: "[pr-watch] PR #{0}: CI red — failing: {1}. Fix the cause, never bypass (QUA-019).",
    waiting: "[pr-watch] PR #{0}: {1} check(s) running…",
  },
};

const fill = (message, ...values) => message.replace(/\{(\d)\}/g, (_, i) => values[i]);

/** Messages in the project's language (French by default). */
export function texts(root = process.cwd()) {
  try {
    return TEXTS[JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr"];
  } catch {
    return TEXTS.fr;
  }
}

/** `gh` with JSON output; null when it fails or answers something unreadable. */
export function ghJson(args, run = spawnSync) {
  const r = run("gh", args, { encoding: "utf8" });
  if (r.status !== 0 && !r.stdout) return null;
  try {
    return JSON.parse(r.stdout);
  } catch {
    return null;
  }
}

/**
 * Bucket of one check of `statusCheckRollup`: a check run (status, conclusion) or a commit status
 * (state). Read from `gh pr view`, which every gh version knows (`gh pr checks --json` came later).
 */
export function bucket(c) {
  if (c.state !== undefined) return c.state === "SUCCESS" ? "pass" : ["PENDING", "EXPECTED"].includes(c.state) ? "pending" : "fail";
  if (c.status !== "COMPLETED") return "pending";
  if (["SUCCESS", "NEUTRAL", "SKIPPED"].includes(c.conclusion)) return "pass";
  return c.conclusion === "CANCELLED" ? "cancel" : "fail";
}

/** One look at the PR: { state: "merged" | "closed" | "green" | "red" | "pending" | "none" | "not-run", … }. */
export function look(pr, gh) {
  const view = gh(["pr", "view", ...(pr ? [pr] : []), "--json", "number,state,statusCheckRollup"]);
  if (!view) return { state: "not-run" };
  if (view.state === "MERGED") return { state: "merged", number: view.number };
  if (view.state === "CLOSED") return { state: "closed", number: view.number };
  const checks = (view.statusCheckRollup ?? []).map((c) => ({ name: c.name ?? c.context, bucket: bucket(c) }));
  if (!checks.length) return { state: "none", number: view.number };
  const failed = checks.filter((c) => c.bucket === "fail" || c.bucket === "cancel").map((c) => c.name);
  if (failed.length) return { state: "red", number: view.number, failed };
  const pending = checks.filter((c) => c.bucket === "pending").length;
  return pending ? { state: "pending", number: view.number, pending } : { state: "green", number: view.number, total: checks.length };
}

/** Waits until the PR's CI is over, at most `timeout` minutes; returns { code, message }. */
export async function watch({
  pr,
  timeout = 30,
  interval = 15_000,
  gh = (a) => ghJson(a),
  sleep,
  now = Date.now,
  log = () => {},
  T = texts(),
}) {
  const wait = sleep ?? ((ms) => new Promise((r) => setTimeout(r, ms)));
  const end = now() + timeout * 60_000;
  for (;;) {
    const s = look(pr, gh);
    if (s.state === "merged") return { code: 0, message: fill(T.merged, s.number) };
    if (s.state === "closed") return { code: 1, message: fill(T.closed, s.number) };
    if (s.state === "green") return { code: 0, message: fill(T.green, s.number, s.total) };
    if (s.state === "red") return { code: 1, message: fill(T.red, s.number, s.failed.join(", ")) };
    if (s.state === "not-run") return { code: 2, message: fill(T.notRun, T.noGh) };
    if (s.state === "none") return { code: 2, message: fill(T.notRun, fill(T.noChecks, s.number)) };
    if (now() >= end) return { code: 2, message: fill(T.notRun, fill(T.timeout, timeout, s.number)) };
    log(fill(T.waiting, s.number, s.pending));
    await wait(interval);
  }
}

/** Command line: `[<pr>] [--timeout <minutes>]`. */
export function parseArgs(argv) {
  const i = argv.indexOf("--timeout");
  const timeout = i >= 0 ? Number(argv[i + 1]) || 30 : 30;
  const pr = argv.find((a, j) => /^\d+$/.test(a) && argv[j - 1] !== "--timeout");
  return { pr, timeout };
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  const interval = Number(process.env.DRWIL_PR_WATCH_INTERVAL) || 15_000;
  const r = await watch({ ...parseArgs(process.argv.slice(2)), interval, log: console.log });
  (r.code === 0 ? console.log : console.error)(r.message);
  process.exitCode = r.code;
}
