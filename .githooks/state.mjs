#!/usr/bin/env node
// Runtime governance state: .drwil/state.json (docs/ia-first.md, "Governance state").
//
// Only reader and only writer of the state: the context reinjected to the agent
// (`node .githooks/state.mjs`, Claude's SessionStart hook), the transitions
// (`node .githooks/state.mjs move <ACTIVITY>`, `drwil state`) and the pre-commit
// import it. No agent tool is required.
// The state is local (gitignored) and only holds references: the allowed scope
// lives in the `scope` block of the active brief, which is versioned.
// Missing file = neutral SCOPING; no active state is ever inferred.
// An invalid state is brought back to neutral AND reported: never hidden, never permissive.
// Closing (VERIFY → CLOSING) requires the evidence of `drwil verify --evidence`: this
// file reads it, it runs no check (a single engine, .githooks/engine.mjs).
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, join, posix } from "node:path";
import { fileURLToPath } from "node:url";
import { BRIEF_DIRS, briefStatus, config, missingCriteria, MODEL_RE, queueable, scopedBriefs } from "./briefs.mjs";
import { closingMerged, closingSignals, noGhText, pruneMergedBranches, prunedMessage } from "./close.mjs";
import { main, readStdinJson } from "./state-cli.mjs";
import { fill, shortReminder, STATE_MESSAGES } from "./state-texts.mjs";

// Unchanged interface for the other hooks, the CLI and the tools: everything this file exported
// before it was split stays importable from here.
export {
  acceptanceCriteria,
  briefMaturity,
  briefStatus,
  declaredContracts,
  hasIncrements,
  openDecisions,
  readyGaps,
  scopedBriefs,
} from "./briefs.mjs";
export { context, governanceDisabled, shortReminder } from "./state-texts.mjs";
// The command line lives in state-cli.mjs (QUA-001: files under 400 lines); `node .githooks/state.mjs`
// stays the entry point.
export { main, readStdinJson };

export const ACTIVITIES = ["SCOPING", "EXPECTATION", "REQUEST", "BUILD", "EVIDENCE", "VERIFY", "CLOSING"];

// One activity at a time; back to BUILD after EVIDENCE or a failed VERIFY;
// abandon to SCOPING from any activity.
const NEXT = {
  SCOPING: ["EXPECTATION"],
  EXPECTATION: ["REQUEST"],
  REQUEST: ["BUILD"],
  BUILD: ["EVIDENCE"],
  EVIDENCE: ["VERIFY", "BUILD"],
  VERIFY: ["CLOSING", "BUILD"],
  CLOSING: ["SCOPING"],
};

export const NEUTRAL_STATE = Object.freeze({ version: 1, activity: "SCOPING", active_brief: null, active_request: null, since: null });

// auto_merge: absent from the neutral state, set only by a human start (`start`).
// queue, queue_merge (QUA-037): the briefs a human queued at start, run next without a new gesture.
// detour (QUA-050): the workstream set aside while a prior defect is fixed (.githooks/detour.mjs).
const FIELDS = new Set([...Object.keys(NEUTRAL_STATE), "auto_merge", "queue", "queue_merge", "detour"]);
const ISO_RE = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}(:\d{2}(\.\d+)?)?(Z|[+-]\d{2}:\d{2})$/;
const texts = (root) => STATE_MESSAGES[config(root).lang === "en" ? "en" : "fr"];

export function transitionAllowed(from, to) {
  if (!ACTIVITIES.includes(from) || !ACTIVITIES.includes(to) || from === to) return false;
  return to === "SCOPING" || NEXT[from].includes(to);
}

/** Problems of the state (in the project's language, empty list if valid). */
export function validateState(state, root) {
  const T = texts(root);
  if (typeof state !== "object" || state === null || Array.isArray(state)) return [T.notObject];
  const problems = [];
  for (const key of Object.keys(state)) if (!FIELDS.has(key)) problems.push(fill(T.unknownField, key));
  if (state.version !== 1) problems.push(T.version);
  if (!ACTIVITIES.includes(state.activity)) {
    problems.push(fill(T.unknownActivity, state.activity, ACTIVITIES.join(", ")));
  } else if (state.activity === "SCOPING") {
    if (state.active_brief !== null || state.active_request !== null) problems.push(T.scopingEmpty);
  } else if (state.active_brief === null || state.active_brief === undefined) {
    problems.push(fill(T.briefRequired, state.activity));
  }
  // A detour's brief is deleted by its own PR, before the resume: its absence is expected too.
  if (state.active_brief !== null && state.active_brief !== undefined)
    problems.push(...validateBrief(state.active_brief, root, state.activity === "CLOSING" || state.detour !== undefined, T));
  if (state.detour !== undefined) problems.push(...validateDetour(state.detour, state.activity, root, T));
  if (state.active_request !== null && typeof state.active_request !== "string") problems.push(T.request);
  if (state.auto_merge !== undefined && typeof state.auto_merge !== "boolean") problems.push(T.autoMergeType);
  else if (state.auto_merge === true && state.activity === "SCOPING") problems.push(T.autoMergeScoping);
  if (state.queue !== undefined) {
    if (!Array.isArray(state.queue) || state.queue.some((b) => typeof b !== "string")) problems.push(T.queueType);
    else for (const b of state.queue) problems.push(...validateBrief(b, root, false, T));
  }
  if (state.queue_merge !== undefined && typeof state.queue_merge !== "boolean") problems.push(T.queueMergeType);
  if (state.since !== null && !(typeof state.since === "string" && ISO_RE.test(state.since) && !Number.isNaN(Date.parse(state.since)))) {
    problems.push(T.since);
  }
  return problems;
}

function validateBrief(brief, root, mayBeMissing, T) {
  if (typeof brief !== "string") return [T.briefType];
  const path = posix.normalize(brief.replace(/\\/g, "/"));
  const dir = BRIEF_DIRS.find((d) => path.startsWith(d));
  if (!dir || !path.endsWith(".md")) return [fill(T.briefPlace, brief, BRIEF_DIRS.join(T.or))];
  if (MODEL_RE.test(posix.basename(path))) return [fill(T.briefModel, brief)];
  // In CLOSING, the "Closing" recipe deletes the brief: its absence is the normal outcome, and
  // refusing it would make the state invalid at the very moment of going back to SCOPING.
  if (!existsSync(join(root, path)) && !mayBeMissing) return [fill(T.briefMissing, brief)];
  return [];
}

// The workstream's brief may be missing on the detour's branch, cut from the main branch.
function validateDetour(d, activity, root, T) {
  const text = (v) => typeof v === "string" && v !== "";
  const shape =
    typeof d === "object" &&
    d !== null &&
    text(d.origin_branch) &&
    text(d.branch) &&
    (d.stash === null || text(d.stash)) &&
    (d.human === undefined || typeof d.human === "boolean");
  if (!shape) return [T.detourType];
  if (activity !== "BUILD") return [T.detourActivity];
  return validateBrief(d.origin_brief, root, true, T);
}

/** `{ state, source: "missing" | "file", problems }`; an invalid state is replaced by the neutral one. */
export function readState(root) {
  const file = join(root, ".drwil", "state.json");
  if (!existsSync(file)) return { state: { ...NEUTRAL_STATE }, source: "missing", problems: [] };
  let state;
  try {
    state = JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return { state: { ...NEUTRAL_STATE }, source: "file", problems: [texts(root).unreadable] };
  }
  const problems = validateState(state, root);
  return problems.length ? { state: { ...NEUTRAL_STATE }, source: "file", problems } : { state, source: "file", problems };
}

// Transitions that open rights: reserved to a human in "human" mode (default).
// The others narrow rights or go back. Closing is no longer one of them
// (docs/ia-first.md, "Starting and closing"): closing removes rights, requires verify
// evidence, and acceptance stays the merge of the PR, a human one.
const HUMAN = new Set(["SCOPING>EXPECTATION", "REQUEST>BUILD"]);

export function requiresHuman(from, to, mode) {
  return mode !== "agent" && HUMAN.has(`${from}>${to}`);
}

/**
 * Latest verify evidence (.drwil/evidence/verify-*.json, outside git): `{ ok: true }`
 * if it allows closing, otherwise `{ ok: false, reason }` (`code` "noEvidence" or "otherCommit" when it is to be renewed). Verdict pass or attested,
 * on the HEAD commit, clean worktree at verify time and now: otherwise, what is
 * closed would not be what was verified.
 */
export function verifyEvidence(root) {
  const T = texts(root);
  const dir = join(root, ".drwil", "evidence");
  // ISO timestamp in the name: alphabetical order is chronological order.
  const names = existsSync(dir)
    ? readdirSync(dir)
        .filter((n) => /^verify-.+\.json$/.test(n))
        .sort()
    : [];
  if (!names.length) return { ok: false, code: "noEvidence", reason: T.noEvidence };
  const name = names.at(-1);
  let ev;
  try {
    ev = JSON.parse(readFileSync(join(dir, name), "utf8"));
  } catch {
    return { ok: false, reason: fill(T.evidenceUnreadable, name) };
  }
  const git = (...args) => {
    const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
    return r.status === 0 ? r.stdout.trim() : null;
  };
  const head = git("rev-parse", "HEAD");
  if (!head) return { ok: false, reason: T.noHead };
  if (ev.commit !== head) return { ok: false, code: "otherCommit", reason: fill(T.otherCommit, name) };
  if (ev.dirtyWorktree !== false || git("status", "--porcelain") !== "") return { ok: false, reason: T.dirty };
  if (ev.status === "pass" || ev.status === "attested") {
    // Evidence without `criteria` (no active brief, or written by an older verify): nothing to judge.
    const unproven = (Array.isArray(ev.criteria) ? ev.criteria : []).filter((c) => c.status !== "pass");
    if (unproven.length) return { ok: false, reason: fill(T.criteria, unproven.map((c) => `« ${c.text} » (${c.status})`).join(", ")) };
    if (Array.isArray(ev.undeclared) && ev.undeclared.length) return { ok: false, reason: fill(T.undeclared, ev.undeclared.join(", ")) };
    return { ok: true };
  }
  if (ev.status === "manual") return { ok: false, reason: T.manual };
  if (ev.status === "fail" || ev.status === "error") return { ok: false, reason: fill(T.failed, ev.status.toUpperCase()) };
  return { ok: false, reason: fill(T.noVerdict, name) };
}

const STARTABLE_FROM = new Set(["SCOPING", "EXPECTATION", "REQUEST"]);

/**
 * Starts a workstream in one gesture: from SCOPING, EXPECTATION or REQUEST, moves straight to
 * BUILD on a scoped brief. In "human" mode (default), `human` must be true: it is only for a
 * human answer to a poll read by the tool's hook, or for a choice made in an interactive
 * terminal. `{ code: 0 | 1, message }`.
 */
export function start(root, brief, { human = false, request, autoMerge = false, queue = [], now = new Date().toISOString() } = {}) {
  const T = texts(root);
  const read = readState(root);
  const origin = read.state.activity;
  // CLOSING, brief deleted and its pull request merged: back to SCOPING by itself, then the start goes on.
  const closing = origin === "CLOSING";
  const from = closing ? "SCOPING" : origin;
  if (!closing && !STARTABLE_FROM.has(from)) return { code: 1, message: fill(T.startFrom, origin) };
  const norm = (b) =>
    posix.normalize(
      String(b ?? "")
        .trim()
        .replace(/\\/g, "/"),
    );
  const path = norm(brief);
  // The head of a queue a human decided is started by the agent, without a new gesture (QUA-037).
  const fromQueue = !human && from === "SCOPING" && read.state.queue?.[0] === path;
  if (config(root).transitions !== "agent" && !human && !fromQueue) return { code: 1, message: T.startHuman };
  if (closing) {
    const active = read.state.active_brief;
    if (existsSync(join(root, active))) return { code: 1, message: fill(T.startClosingBrief, active) };
    const refused = { no: T.startClosingOpen, unknown: `${T.startClosingUnknown} — ${noGhText(root)}` }[closingMerged(root)];
    if (refused) return { code: 1, message: refused };
  }
  if (!scopedBriefs(root).some((b) => b.path === path)) return { code: 1, message: fill(T.startNotScoped, brief) };
  const rest = human ? queue.map(norm) : fromQueue ? read.state.queue.slice(1) : [];
  const queued = rest.length || fromQueue ? [path, ...rest] : [];
  const notReady = queued.find((b) => !queueable(root, b));
  if (notReady) return { code: 1, message: fill(T.queueNotReady, notReady) };
  const missing = missingCriteria(root, path);
  if (missing === "blocking") return { code: 1, message: fill(T.startNoCriteria, path) };
  const state = { version: 1, activity: "BUILD", active_brief: path, active_request: request ?? null, since: now };
  // Authorising the merge is an acceptance decision: human only, even in agent mode; a queue carries it.
  if ((autoMerge === true && human) || (fromQueue && read.state.queue_merge === true)) state.auto_merge = true;
  if (rest.length) Object.assign(state, { queue: rest, queue_merge: human ? autoMerge === true : read.state.queue_merge === true });
  const problems = validateState(state, root);
  if (problems.length) return { code: 1, message: fill(T.startInvalid, problems.join(" ; ")) };
  mkdirSync(join(root, ".drwil"), { recursive: true });
  writeFileSync(join(root, ".drwil", "state.json"), JSON.stringify(state, null, 2) + "\n");
  const warned = missing ? `\n${fill(T.noCriteria, path)}` : "";
  const next = rest.length ? `\n${fill(T.queued, rest.join(", "))}` : "";
  // The previous workstreams' branches, merged, are no longer needed (QUA-048).
  const pruned = prunedMessage(root, pruneMergedBranches(root));
  return { code: 0, message: fill(T.started, origin, path, state.auto_merge ? T.mergeAuthorised : "") + next + warned + pruned };
}

/** New state after the transition to `to`, or `{ error }`; writes nothing. */
export function prepareTransition(read, to, { brief, request, now }, root) {
  const T = texts(root);
  const from = read.state.activity;
  if (!transitionAllowed(from, to)) {
    const allowed = ACTIVITIES.filter((a) => transitionAllowed(from, a)).join(", ");
    return { error: fill(T.forbidden, from, to, allowed) };
  }
  if (brief !== undefined && to !== "EXPECTATION") return { error: T.briefOption };
  // Abandoning to SCOPING stays possible; the set-aside work stays in `git stash list`.
  if (read.state.detour && to !== "SCOPING") return { error: T.detourMove };
  let state;
  if (to === "SCOPING") {
    state = { ...NEUTRAL_STATE, since: now };
    // A queue goes on after a closing; any other return to SCOPING abandons it (QUA-037).
    if (from === "CLOSING" && read.state.queue?.length)
      Object.assign(state, { queue: read.state.queue, queue_merge: read.state.queue_merge === true });
  } else if (to === "EXPECTATION") {
    if (!brief) return { error: T.briefNeeded };
    state = {
      version: 1,
      activity: "EXPECTATION",
      active_brief: posix.normalize(brief.replace(/\\/g, "/")),
      active_request: request ?? null,
      since: now,
    };
  } else {
    state = { ...read.state, activity: to, active_request: request ?? read.state.active_request, since: now };
  }
  const problems = validateState(state, root);
  return problems.length ? { error: problems.join(" ; ") } : { state };
}

/**
 * Applies a transition and writes the state. `{ code: 0 | 1, message }`.
 * `confirm(question)` is only called if a human is required, and only in front of
 * an interactive terminal: without a terminal, a human transition is refused.
 */
export async function move(root, { to, brief, request }, { tty = false, confirm, now = new Date().toISOString() } = {}) {
  const T = texts(root);
  const read = readState(root);
  const resumed = read.problems.length ? fill(T.resumed, read.problems.join(" ; ")) : "";
  const prep = prepareTransition(read, to, { brief, request, now }, root);
  if (prep.error) return { code: 1, message: resumed + prep.error };
  const from = read.state.activity;
  // Before the human confirmation: no point asking for a decision that the evidence refuses.
  // Also in agent mode: the "transitions" setting says who decides, not on what.
  const activeBrief = read.state.active_brief && join(root, read.state.active_brief);
  let warning = "";
  if (to === "CLOSING") {
    const evidence = verifyEvidence(root);
    if (!evidence.ok) return { code: 1, message: resumed + fill(T.closingRefused, evidence.reason) };
    // Every workstream brief has acceptance criteria (QUA-021): refused if the project enforces recent
    // requirements, a warning otherwise.
    const missing = missingCriteria(root, read.state.active_brief);
    if (missing === "blocking") return { code: 1, message: resumed + fill(T.closingNoCriteria, read.state.active_brief) };
    if (missing) warning = "\n" + fill(T.noCriteria, read.state.active_brief);
  }
  // Leaving BUILD: last moment to fix code cheaply, so the closing's pitfalls are reported now (QUA-048).
  const signals = to === "EVIDENCE" && activeBrief ? closingSignals(root, read.state.active_brief) : [];
  if (signals.length) warning += `\n${T.closingSignals}\n${signals.map((s) => `  ⚠ ${s}`).join("\n")}`;
  if (
    from === "CLOSING" &&
    to === "SCOPING" &&
    activeBrief &&
    existsSync(activeBrief) &&
    briefStatus(readFileSync(activeBrief, "utf8")) === "done"
  ) {
    return { code: 1, message: resumed + fill(T.stillThere, read.state.active_brief) };
  }
  if (requiresHuman(from, to, config(root).transitions)) {
    if (!tty || typeof confirm !== "function") return { code: 1, message: resumed + fill(T.humanDecision, from, to) };
    if (!(await confirm(fill(T.confirm, from, to)))) return { code: 1, message: T.confirmRefused };
  }
  mkdirSync(join(root, ".drwil"), { recursive: true });
  writeFileSync(join(root, ".drwil", "state.json"), JSON.stringify(prep.state, null, 2) + "\n");
  return { code: 0, message: `${resumed}${from} → ${to}${warning}` };
}

/** Stops the queue (QUA-037): the active workstream goes on, nothing is started after it. */
export function stopQueue(root) {
  const T = texts(root);
  const read = readState(root);
  if (!read.state.queue) return { code: 0, message: T.noQueue };
  const state = { ...read.state };
  delete state.queue;
  delete state.queue_merge;
  writeFileSync(join(root, ".drwil", "state.json"), JSON.stringify(state, null, 2) + "\n");
  return { code: 0, message: fill(T.queueStopped, read.state.queue.join(", ")) };
}

/**
 * Points the state at `brief` with the `detour` record, or back without it (`detour` null), QUA-050.
 * The only writer for .githooks/detour.mjs; an invalid result is a bug of the caller: it throws.
 */
export function setDetour(root, brief, detour, now = new Date().toISOString()) {
  const state = { ...readState(root).state, active_brief: brief, since: now };
  if (detour) state.detour = detour;
  else delete state.detour;
  const problems = validateState(state, root);
  if (problems.length) throw new Error(problems.join(" ; "));
  writeFileSync(join(root, ".drwil", "state.json"), JSON.stringify(state, null, 2) + "\n");
}

const REMINDER_FILE = [".drwil", "reminder.json"];

/**
 * The reminder only if it changed since the last one injected (fingerprint in .drwil/reminder.json,
 * local like the state); null otherwise. Missing or unreadable fingerprint: the reminder is injected.
 */
export function reminderIfChanged(root) {
  const text = shortReminder(readState(root), root);
  if (text === null) return null;
  const fingerprint = createHash("sha256").update(text).digest("hex");
  const file = join(root, ...REMINDER_FILE);
  try {
    if (JSON.parse(readFileSync(file, "utf8")).fingerprint === fingerprint) return null;
  } catch {}
  try {
    mkdirSync(join(root, ".drwil"), { recursive: true });
    writeFileSync(file, JSON.stringify({ fingerprint }) + "\n");
  } catch {}
  return text;
}

/**
 * Root of the command: the parent of .githooks/, like run-checks (engine.mjs, projectRoot — not
 * imported: scope-gate.mjs imports this file), never the current folder: the agent's hooks call it
 * from wherever the agent `cd`-ed. `DRWIL_ROOT` replaces it, for tests only.
 */
export function stateRoot(scriptUrl = import.meta.url, env = process.env) {
  return env.DRWIL_ROOT || join(dirname(fileURLToPath(scriptUrl)), "..");
}

// Real paths on both sides: a project path through a symbolic link must still run the command.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  process.exitCode = await main(process.argv.slice(2), stateRoot());
}
