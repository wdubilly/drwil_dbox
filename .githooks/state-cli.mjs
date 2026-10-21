// Command line of the governance state (split from state.mjs, which stays the entry point:
// `node .githooks/state.mjs …`, `drwil state …`).
import { config, scopedBriefs } from "./briefs.mjs";
import { ACTIVITIES, move, readState, reminderIfChanged, start, stopQueue } from "./state.mjs";
import { context, fill, STATE_MESSAGES } from "./state-texts.mjs";

const texts = (root) => STATE_MESSAGES[config(root).lang === "en" ? "en" : "fr"];

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
    if (r.code === 0) console.log(`✓ ${r.message}\n\n${context(readState(root), root)}`);
    else console.error(`✗ ${r.message}`);
    return r.code;
  }
  // The queue (QUA-037): `next` starts its head (allowed to the agent: the human decided the queue),
  // `queue stop` drops what is left.
  if (args[0] === "next" || (args[0] === "queue" && args[1] === "stop")) {
    const r = args[0] === "next" ? start(root, readState(root).state.queue?.[0]) : stopQueue(root);
    if (r.code === 0) console.log(`✓ ${r.message}\n\n${context(readState(root), root)}`);
    else console.error(`✗ ${r.message}`);
    return r.code;
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
  const r = await move(root, { to, brief: option(args, "--brief"), request: option(args, "--request") }, { tty, confirm });
  if (r.code === 0) console.log(`✓ ${r.message}\n\n${context(readState(root), root)}`);
  else console.error(`✗ ${r.message}`);
  return r.code;
}
