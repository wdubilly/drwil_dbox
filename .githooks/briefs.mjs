// Repository reading for the governance state (split from state.mjs, which re-exports it): settings,
// status and list of scoped briefs, priorities from the committed index.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { readBlock } from "./scope.mjs";

export const BRIEF_DIRS = ["docs/projects/"];
export const MODEL_RE = /^modele-|^model-/;
// Routine maintenance (LOW risk, DRWIL-012) is launched like a brief but is not a workstream: no
// acceptance criteria are required from it.
export const ROUTINE_RE = /(^|\/)(routine-maintenance|entretien-courant)\.md$/;

/** Project settings (.drwil/ia-first.json); unreadable or missing: none. */
export function config(root) {
  try {
    return JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8")) ?? {};
  } catch {
    return {};
  }
}

const STATUS_LINE_RE = /^\*\*(?:Statut|Status)\*\*\s*:\s*(.*)$/;

/**
 * "done", "reference" or "open" — single definition, shared with check-docs.mjs.
 * Only an explicit marker at the start of the status counts: a word searched anywhere ("essai réel
 * fait", "fait (lot 1)" of a brief with several increments) gave false "done".
 */
export function briefStatus(text) {
  const status =
    text
      .split(/\r?\n/)
      .slice(0, 15)
      .map((l) => STATUS_LINE_RE.exec(l))
      .find(Boolean)?.[1] ?? "";
  if (/^(?:terminé\s+le|done\s+on)\s+\d{4}-\d{2}-\d{2}\b/i.test(status)) return "done";
  if (/^(?:référence|reference)\b/i.test(status)) return "reference";
  return "open";
}

const MATURITY_LINE_RE = /^\*\*(?:Maturité|Maturity)\*\*\s*:\s*(.*)$/;

/**
 * Explicit maturity of a brief: "ready", "refine", "in-progress", or "unset" (field absent or
 * unknown: existing briefs and installed projects keep working). Same header window as the status.
 */
export function briefMaturity(text) {
  const value =
    text
      .split(/\r?\n/)
      .slice(0, 15)
      .map((l) => MATURITY_LINE_RE.exec(l))
      .find(Boolean)?.[1] ?? "";
  if (/^(?:prêt|ready)\b/i.test(value)) return "ready";
  if (/^(?:à affiner|to refine)\b/i.test(value)) return "refine";
  if (/^(?:en cours|in progress)\b/i.test(value)) return "in-progress";
  return "unset";
}

const DEFAULT_INDEX = ["docs/projects/pending.md"];
const MATURITY_RANK = { ready: 0, "in-progress": 1, unset: 1, refine: 2 };
const DEFAULT_PRIORITY = 2;

/** Priority of each brief cited by an open item of the committed index ([P0] to [P3], [P2] without a tag). */
function indexPriorities(root) {
  const priorities = new Map();
  for (const path of [config(root).dirs?.index, ...DEFAULT_INDEX].filter(Boolean)) {
    const r = spawnSync("git", ["show", `HEAD:${path}`], { cwd: root, encoding: "utf8" });
    if (r.status !== 0) continue;
    // An item: its "- [ ]" line and its continuation lines; a checked item of a brief with several
    // increments says nothing about the priority of the remaining work.
    for (const item of r.stdout.split(/\r?\n(?=\s*- \[)/)) {
      if (!/^\s*- \[ \]/.test(item)) continue;
      const p = /\[P([0-3])\]/.exec(item)?.[1];
      for (const m of item.matchAll(/`(docs\/projects\/[^`]+\.md)`/g)) {
        if (!priorities.has(m[1])) priorities.set(m[1], p === undefined ? DEFAULT_PRIORITY : Number(p));
      }
    }
    break;
  }
  return priorities;
}

/** `[decision]` items still open in the "Points to decide" section ("Points à trancher"). */
export function openDecisions(text) {
  let inside = false;
  let n = 0;
  for (const line of text.split(/\r?\n/)) {
    if (/^##\s/.test(line)) inside = /^##\s*(\d+\.\s*)?(Points à trancher|Points to decide)\b/i.test(line);
    else if (inside && /^\s*-\s*\[(décision|decision)\]/i.test(line)) n++;
  }
  return n;
}

/** A brief lists its increments as "- **Lot …" or "- **Increment …" bullets. */
export function hasIncrements(text) {
  return /^\s*-\s*\*\*(Lot|Increment|Incrément)\b/im.test(text);
}

/**
 * Can this brief enter a queue (QUA-037)? Scoped, maturity « ready », no open decision, and
 * acceptance criteria: a queue runs without a human, so only what needs no decision.
 */
export function queueable(root, brief) {
  const b = scopedBriefs(root).find((x) => x.path === brief);
  return Boolean(b?.ready && b.maturity === "ready" && acceptanceCriteria(readFileSync(join(root, brief), "utf8")).length);
}

/**
 * A workstream brief without any acceptance criterion (QUA-021): null if it has some, if it is routine
 * maintenance or cannot be read; otherwise "blocking" or "warning", per `newRequirements` (an already
 * equipped project is warned, not broken).
 */
export function missingCriteria(root, brief) {
  const p = join(root, brief ?? "");
  if (!brief || ROUTINE_RE.test(brief) || !existsSync(p) || acceptanceCriteria(readFileSync(p, "utf8")).length) return null;
  return config(root).newRequirements === "blocking" ? "blocking" : "warning";
}

/**
 * Acceptance criteria of a brief: one bullet per criterion in the "Critères d'acceptation" section.
 * `[{ text, link, human }]`: `link` is the backticked contract or check ID of the line (null when
 * absent: a criterion without proof, for the evaluation to refuse), `human` is set by `[humain]`.
 */
export function acceptanceCriteria(text) {
  // A criterion wrapped over several lines keeps its proof at the end: read each bullet whole.
  const items = [];
  let inside = false;
  let open = false;
  for (const line of text.split(/\r?\n/)) {
    if (/^##\s/.test(line)) {
      inside = /^##\s*(\d+\.\s*)?(Critères d'acceptation|Acceptance criteria)\b/i.test(line);
      open = false;
      continue;
    }
    const item = inside ? /^\s*-\s+(.+)$/.exec(line) : null;
    if (item) items.push(item[1]);
    else if (open && /^\s+\S/.test(line)) items[items.length - 1] += ` ${line.trim()}`;
    open = Boolean(item) || (open && /^\s+\S/.test(line));
  }
  const criteria = [];
  for (const item of items) {
    const human = /\[(humain|human)\]/i.test(item);
    // The proof is the last backticked ID: code quoted earlier in the sentence (`init`) is not one.
    const proof = /`([A-Za-z][\w.:-]*)`[^`]*$/.exec(item);
    const link = proof?.[1] ?? null;
    const label = (proof ? item.slice(0, proof.index) + item.slice(proof.index + proof[1].length + 2) : item)
      .replace(/\[(humain|human)\]/gi, "")
      .replace(/`([^`]*)`/g, "$1")
      .replace(/\s*[—–:-]\s*$/, "")
      .replace(/\s+/g, " ")
      .trim();
    if (label) criteria.push({ text: label, link, human });
  }
  return criteria;
}

/**
 * Contracts the brief declares as changed on purpose (« Contrats modifiés » section): the ID in backticks
 * at the head of each bullet. A contract modified in the branch without this declaration is refused.
 */
export function declaredContracts(text) {
  const ids = [];
  let inside = false;
  for (const line of text.split(/\r?\n/)) {
    if (/^##\s/.test(line)) inside = /^##\s*(\d+\.\s*)?(Contrats modifiés|Modified contracts)\b/i.test(line);
    else if (inside) {
      const id = /^\s*-\s+`([A-Za-z][\w.:-]*)`/.exec(line)?.[1];
      if (id) ids.push(id);
    }
  }
  return ids;
}

/**
 * What a brief marked "ready" still lacks, as check-docs message keys: an open [decision], no
 * increment, or no acceptance criterion (nothing would be left to replay as non-regression).
 */
export function readyGaps(text) {
  const gaps = [];
  if (openDecisions(text) > 0) gaps.push("readyUndecided");
  if (!hasIncrements(text)) gaps.push("readyNoIncrement");
  if (!acceptanceCriteria(text).length) gaps.push("readyNoCriteria");
  return gaps;
}

/**
 * Briefs that can be started: briefs of docs/projects/, models excluded, not
 * finished, whose `scope` block exists in HEAD — the validated scope is the committed one,
 * as for the scope gate. `[{ path, title, priority, decisions, ready, maturity }]`: ready ones first (no
 * open decision), then by maturity ("ready", then "in-progress" and "unset", "refine" last), then by
 * index priority, then by path. A "refine" brief is listed, never hidden.
 */
export function scopedBriefs(root) {
  const priorities = indexPriorities(root);
  const briefs = [];
  for (const dir of BRIEF_DIRS) {
    const abs = join(root, dir);
    if (!existsSync(abs)) continue;
    for (const name of readdirSync(abs).sort()) {
      const path = dir + name;
      if (!name.endsWith(".md") || MODEL_RE.test(name)) continue;
      const r = spawnSync("git", ["show", `HEAD:${path}`], { cwd: root, encoding: "utf8" });
      if (r.status !== 0 || !readBlock(r.stdout).patterns.length) continue;
      // A reference brief (kit mechanics) keeps its scope without being a workstream to start.
      if (briefStatus(r.stdout) !== "open") continue;
      const decisions = openDecisions(r.stdout);
      const priority = priorities.get(path) ?? DEFAULT_PRIORITY;
      briefs.push({
        path,
        title: /^#\s+(.+)$/m.exec(r.stdout)?.[1]?.trim() ?? path,
        priority: `P${priority}`,
        decisions,
        ready: decisions === 0,
        maturity: briefMaturity(r.stdout),
      });
    }
  }
  return briefs.sort(
    (a, b) =>
      Number(b.ready) - Number(a.ready) ||
      MATURITY_RANK[a.maturity] - MATURITY_RANK[b.maturity] ||
      a.priority.localeCompare(b.priority) ||
      a.path.localeCompare(b.path),
  );
}

// The brief → intent loop (QUA-036): a workstream brief names the intent it comes from; at closing,
// that intent is updated, or summarised in the journal and removed when fully covered.
const INTENT_FIELD = /^\*\*(?:Intention|Intent)\*\*\s*:\s*`([^`]+)`/m;

/** Path of the intent a brief comes from (`**Intention**: `docs/intentions/x.md``), or null. */
export function briefIntent(text) {
  return INTENT_FIELD.exec(text)?.[1] ?? null;
}

/** Files the branch changed since its merge base with the main branch (DRWIL_BASE, origin/HEAD, then the usual names); [] without a base. */
function branchFiles(root) {
  const git = (...args) => {
    const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
    return r.status === 0 ? r.stdout.trim() : "";
  };
  const refs = [
    process.env.DRWIL_BASE,
    git("symbolic-ref", "--quiet", "refs/remotes/origin/HEAD"),
    "origin/master",
    "origin/main",
    "master",
    "main",
  ];
  for (const ref of refs) {
    if (!ref || !git("rev-parse", "--verify", "--quiet", `${ref}^{commit}`)) continue;
    const base = git("merge-base", "HEAD", ref);
    if (base) return git("diff", "--name-only", "--no-renames", `${base}..HEAD`).split("\n").filter(Boolean);
  }
  return [];
}

/**
 * At commit time: the briefs this commit deletes whose intent still exists but was not touched by
 * the commit nor, earlier, by the branch (compared from the merge base) — `[{ brief, intent }]`.
 * A warning, never a refusal: an intent may already be up to date.
 */
export function intentsLeftBehind(root, staged) {
  const r = spawnSync("git", ["diff", "--cached", "--name-only", "--diff-filter=D", "--no-renames", "-z"], { cwd: root, encoding: "utf8" });
  if (r.status !== 0) return [];
  const deleted = r.stdout.split("\0").filter((f) => f && BRIEF_DIRS.some((d) => f.startsWith(d)) && f.endsWith(".md"));
  if (!deleted.length) return [];
  const touched = new Set([...staged, ...branchFiles(root)]);
  const left = [];
  for (const brief of deleted) {
    // A staged deletion always exists in HEAD: its text is there to read.
    const intent = briefIntent(spawnSync("git", ["show", `HEAD:${brief}`], { cwd: root, encoding: "utf8" }).stdout);
    if (intent && existsSync(join(root, intent)) && !touched.has(intent)) left.push({ brief, intent });
  }
  return left;
}

const REVIEW_DATES =
  /^\*\*(?:Statut|Status)\*\*[^\n]*?(\d{4}-\d{2}-\d{2})|^\*\*(?:Revu le|Reviewed on)\s+(\d{4}-\d{2}-\d{2})\*\*|^##\s*(?:Existant|Current state)[^\n]*?(\d{4}-\d{2}-\d{2})/gm;

/** Latest dated review of an intent: its Status line, a « Revu le … » line or a dated « Existant » heading; null if none. */
export function lastReview(text) {
  const dates = [...text.matchAll(REVIEW_DATES)].map((m) => m[1] ?? m[2] ?? m[3]);
  return dates.length ? dates.sort().at(-1) : null;
}

/** Intents whose latest review is `days` old or more — `[{ file, days }]`, the oldest first. */
export function staleIntents(root, cfg, now = Date.now(), days = 30) {
  const dir = cfg?.dirs?.intentions ?? "docs/intentions";
  if (!existsSync(join(root, dir))) return [];
  const stale = [];
  for (const f of readdirSync(join(root, dir)).filter((f) => f.endsWith(".md") && !/^readme\.md$/i.test(f))) {
    const date = lastReview(readFileSync(join(root, dir, f), "utf8"));
    const age = date ? Math.floor((now - new Date(date).getTime()) / 86400000) : null;
    if (age !== null && age >= days) stale.push({ file: `${dir}/${f}`, days: age });
  }
  return stale.sort((a, b) => b.days - a.days);
}
