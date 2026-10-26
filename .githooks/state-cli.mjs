// Command line of the governance state (split from state.mjs, which stays the entry point:
// `node .githooks/state.mjs …`, `drwil state …`), the reminder and the reopening of a closed workstream.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { config, scopedBriefs } from "./briefs.mjs";
import { ghRun, mainBranch } from "./close.mjs";
import { ACTIVITIES, move, readState, start, stopQueue, writeState } from "./state.mjs";
import { context, fill, shortReminder, STATE_MESSAGES } from "./state-texts.mjs";

const texts = (root) => STATE_MESSAGES[config(root).lang === "en" ? "en" : "fr"];

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
 * Reopens a closed workstream whose pull request revealed a defect before its merge (QUA-116): from
 * CLOSING, on the workstream's branch, clean worktree, its PR open. The automatic merge is cancelled
 * first (a branch whose merge is scheduled receives no push); the brief comes back as it was before the
 * commit that deleted it, in a commit of its own (hooks run); then BUILD on it, `reopened` set, the
 * merge authorisation and the queue kept. History is never rewritten, nothing is pushed. Refused
 * without changing anything otherwise; a failed git or gh call refuses, never lets through (QUA-013).
 * `{ code, message }`.
 */
export function reopen(root, { gh = ghRun, now = new Date().toISOString() } = {}) {
  const T = texts(root);
  const refused = (key, ...args) => ({ code: 1, message: fill(T[key], ...args) });
  const raw = (...args) => {
    const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
    return r.status === 0 ? r.stdout : null;
  };
  const git = (...args) => raw(...args)?.trim() ?? null;
  const { state } = readState(root);
  const brief = state.active_brief;
  if (state.activity !== "CLOSING" || !brief) return refused("reopenFrom", state.activity);
  if (git("status", "--porcelain") !== "") return refused("reopenDirty");
  const main = mainBranch(root);
  const branch = git("symbolic-ref", "--quiet", "--short", "HEAD");
  if (!main || !branch || branch === main) return refused("reopenBranch");
  const view = () => {
    try {
      const pr = JSON.parse(gh(root, ["pr", "view", branch, "--json", "number,state,autoMergeRequest"]) ?? "");
      return pr && Number.isInteger(pr.number) && typeof pr.state === "string" ? pr : null;
    } catch {
      return null;
    }
  };
  const pr = view();
  if (!pr) return refused("reopenUnknown", branch);
  if (pr.state !== "OPEN") return refused("reopenNotOpen", pr.number, pr.state);
  // The brief before the closing: the parent of the branch's commit that deleted it; still in HEAD, nothing to restore.
  let text = null;
  if (git("cat-file", "-e", `HEAD:${brief}`) === null) {
    const closing = git("log", "-1", "--diff-filter=D", "--format=%H", `${main}..HEAD`, "--", brief);
    text = closing ? raw("show", `${closing}^:${brief}`) : null;
    if (!text) return refused("reopenNoBrief", brief, branch);
  }
  let cancelled = "";
  if (pr.autoMergeRequest) {
    const after = gh(root, ["pr", "merge", String(pr.number), "--disable-auto"]) === null ? null : view();
    if (!after || after.autoMergeRequest) return refused("reopenAutoMerge", pr.number);
    cancelled = `\n${T.reopenCancelled}`;
  }
  if (text !== null) {
    writeFileSync(join(root, brief), text);
    let r;
    for (const args of [
      ["add", "--", brief],
      ["commit", "-q", "-m", fill(T.reopenMessage, brief, pr.number), "--", brief],
    ]) {
      r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
      if (r.status !== 0) break;
    }
    if (r.status !== 0) {
      raw("reset", "-q", "--", brief);
      unlinkSync(join(root, brief));
      return { code: 1, message: fill(T.reopenCommitFailed, r.stderr.trim().split("\n").at(-1)) + cancelled };
    }
  }
  writeState(root, { ...state, activity: "BUILD", since: now, reopened: { pr: pr.number } });
  const restored = text === null ? "" : `\n${fill(T.reopenRestored, brief, git("rev-parse", "--short", "HEAD"))}`;
  return { code: 0, message: fill(T.reopened, brief, pr.number) + restored + cancelled };
}

/** Value of the `--name value` option in `args`, or undefined. */
function option(args, name) {
  const i = args.indexOf(name);
  return i >= 0 && i + 1 < args.length ? args[i + 1] : undefined;
}

/**
 * JSON read on `stream`, as soon as it is complete; null if it is empty, invalid or still open after `ms`.
 * A hook never waits for an input that does not close: the tool would let the call through at its deadline.
 */
export function readStdinJson(stream = process.stdin, ms = 2000) {
  return new Promise((resolve) => {
    let raw = "";
    const finish = () => {
      clearTimeout(timer);
      stream.pause();
      try {
        resolve(JSON.parse(raw));
      } catch {
        resolve(null);
      }
    };
    const timer = setTimeout(finish, ms);
    stream.on("data", (d) => {
      raw += d;
      try {
        JSON.parse(raw);
        finish();
      } catch {}
    });
    stream.on("end", finish);
  });
}

/** Prints the outcome of a transition (the new context on success); returns the exit code. */
function report(r, root) {
  if (r.code === 0) console.log(`✓ ${r.message}\n\n${context(readState(root), root)}`);
  else console.error(`✗ ${r.message}`);
  return r.code;
}

/** Entry point shared by `node .githooks/state.mjs` and `drwil state`. Returns the exit code. */
export async function main(args, root, input = process.stdin) {
  if (args[0] === "briefs") {
    const briefs = scopedBriefs(root);
    if (args.includes("--json")) console.log(JSON.stringify(briefs, null, 2));
    else console.log(briefs.length ? briefs.map((b, i) => `${i + 1}. ${b.path} — ${b.title}`).join("\n") : texts(root).noBriefs);
    return 0;
  }
  if (args[0] === "start") {
    const T = texts(root);
    const briefs = scopedBriefs(root);
    const tty = Boolean(process.stdin.isTTY && process.stdout.isTTY);
    let choice = args[1];
    let human = false;
    let autoMerge = false;
    if (tty) {
      // An interactive terminal: the human chooses themselves (never a preselected choice).
      const { createInterface } = await import("node:readline/promises");
      const rl = createInterface({ input: process.stdin, output: process.stdout });
      try {
        if (!choice) {
          console.log(
            briefs
              .map(
                (b, i) =>
                  `${i + 1}. ${b.path} — ${b.title} [${b.priority}${b.ready ? "" : fill(T.notReady, b.decisions)}${b.maturity === "refine" ? T.refine : ""}]`,
              )
              .join("\n"),
          );
          const n = Number((await rl.question(T.pick)).trim());
          choice = briefs[n - 1]?.path;
        }
        human = choice !== undefined && (await rl.question(fill(T.confirmStart, choice))).trim() === "START";
        if (human) autoMerge = (await rl.question(T.askMerge)).trim() === "MERGE";
      } finally {
        rl.close();
      }
      if (!human) {
        console.error(T.notConfirmed);
        return 1;
      }
    }
    const r = start(root, choice, { human, autoMerge, request: option(args, "--request") });
    return report(r, root);
  }
  // The queue (QUA-037): `next` starts its head (allowed to the agent: the human decided the queue),
  // `queue stop` drops what is left.
  if (args[0] === "next" || (args[0] === "queue" && args[1] === "stop")) {
    const r = args[0] === "next" ? start(root, readState(root).state.queue?.[0]) : stopQueue(root);
    return report(r, root);
  }
  if (args[0] === "reopen") {
    const r = reopen(root);
    return report(r, root);
  }
  if (args[0] === "reminder") {
    // Prompt hook (Claude Code: UserPromptSubmit): writes only if the state changed, never breaks a prompt.
    try {
      const text = reminderIfChanged(root);
      if (text) console.log(text);
    } catch {}
    return 0;
  }
  if (args[0] === "copilot-start" || args[0] === "copilot-message") {
    // Copilot CLI (native hooks, .github/hooks/drwil.json) reads JSON, not raw text. Any failure: no context.
    let out = {};
    try {
      if (args[0] === "copilot-start") out = { additionalContext: context(readState(root), root) };
      else {
        const prompt = (await readStdinJson(input))?.transformedPrompt;
        const text = typeof prompt === "string" ? reminderIfChanged(root) : null;
        if (text) out = { modifiedTransformedPrompt: `${text}\n\n${prompt}` };
      }
    } catch {}
    console.log(JSON.stringify(out));
    return 0;
  }
  if (args[0] === "gemini-start" || args[0] === "gemini-message") {
    // Gemini CLI (.gemini/settings.json) reads JSON on stdout, not raw text. Any failure: no context.
    const hookEventName = args[0] === "gemini-start" ? "SessionStart" : "BeforeAgent";
    let out = {};
    try {
      const text = hookEventName === "SessionStart" ? context(readState(root), root) : reminderIfChanged(root);
      if (text) out = { hookSpecificOutput: { hookEventName, additionalContext: text } };
    } catch {}
    console.log(JSON.stringify(out));
    return 0;
  }
  if (args[0] !== "move") {
    // Reading: never breaks an agent's session start.
    try {
      const read = readState(root);
      console.log(args.includes("--json") ? JSON.stringify(read, null, 2) : context(read, root));
    } catch (e) {
      console.log(fill(texts(root).unreadableState, e.message));
    }
    return 0;
  }
  const to = args[1];
  if (!to || to.startsWith("--")) {
    console.error(fill(texts(root).usage, ACTIVITIES.join("|")));
    return 2;
  }
  const tty = Boolean(process.stdin.isTTY && process.stdout.isTTY);
  const confirm = async (question) => {
    const { createInterface } = await import("node:readline/promises");
    const rl = createInterface({ input: process.stdin, output: process.stdout });
    try {
      return (await rl.question(question)).trim() === to;
    } finally {
      rl.close();
    }
  };
  return report(await move(root, { to, brief: option(args, "--brief"), request: option(args, "--request") }, { tty, confirm }), root);
}
