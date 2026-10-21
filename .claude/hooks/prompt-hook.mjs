#!/usr/bin/env node
// Starting a workstream from the chat (`/drwil-start`, docs/ia-first.md, "Starting and closing"):
// the human decision is the answer to a poll, read by the tool, never by the model.
// - PreToolUse (poll): refuses a start poll that arrives with an answer already filled in or a
//   default value — the model does not answer in the human's place and preselects nothing (its
//   recommendation goes through the "(Recommended)" label).
// - PostToolUse (poll): reads the human's answer and starts the chosen brief
//   (`.githooks/state.mjs`, the only logic, valid for any tool). A multiple-choice poll queues the
//   chosen briefs, in the order of the poll's options, never in the order of the clicks: the agent
//   starts the next one after each closing (QUA-037).
// The same hooks serve Claude Code and Copilot CLI, which reads this settings file; only the poll
// formats differ. A poll that is not drwil's passes through untouched.
import { readFileSync, realpathSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const MARK = "drwil-start";
// Optional second question (Claude Code) or field (Copilot CLI) of the same poll: authorise the automatic merge.
const MERGE_MARK = "drwil-merge";
const MERGE_YES_RE = /^(fusion automatique|automatic merge)\b/i;
// With a single scoped brief, the poll's second option (a question needs two): start nothing.
const NOT_NOW_RE = /^(pas maintenant|not now)\b/i;
// The folder holding this hook's .claude/, never the current folder: it follows the agent's `cd`.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

const TEXTS = {
  fr: {
    refused: "drwil : sondage de lancement refusé — il ne doit contenir ni réponse ni valeur par défaut ; c'est l'humain qui choisit.",
    notNow: "rien n'est lancé (réponse « Pas maintenant ») : l'état est inchangé.",
    noAnswer:
      "aucune réponse lue dans le sondage de lancement : rien n'est lancé, l'état est inchangé. Repli : `node .githooks/state.mjs start` dans un terminal interactif.",
    error: (m) =>
      `erreur au lancement (${m}) : rien n'est lancé, l'état est inchangé. Repli : \`node .githooks/state.mjs start\` dans un terminal interactif.`,
  },
  en: {
    refused: "drwil: start poll refused — it must contain neither an answer nor a default value; the human chooses.",
    notNow: "nothing started (answer « Not now »): the state is unchanged.",
    noAnswer:
      "no answer read in the start poll: nothing started, the state is unchanged. Fallback: `node .githooks/state.mjs start` in an interactive terminal.",
    error: (m) =>
      `error while starting (${m}): nothing started, the state is unchanged. Fallback: \`node .githooks/state.mjs start\` in an interactive terminal.`,
  },
};

/** What the agent reads after the poll: never silent on a drwil start poll. */
function tell(text) {
  console.log(JSON.stringify({ hookSpecificOutput: { hookEventName: "PostToolUse", additionalContext: `[drwil] ${text}` } }));
}

/** The start poll, in one of the two known forms; null otherwise. */
export function startPoll(input) {
  const e = input?.tool_input ?? {};
  // Claude Code: questions[{ question, header, options }], answers in `answers`.
  const q = Array.isArray(e.questions) ? e.questions.find((x) => x?.header === MARK) : null;
  const qMerge = q ? e.questions.find((x) => x?.header === MERGE_MARK) : null;
  if (q)
    return {
      form: "claude",
      question: q.question,
      mergeQuestion: qMerge?.question ?? null,
      options: Array.isArray(q.options) ? q.options.map((o) => o?.label) : [],
      prefilled: Boolean(e.answers && Object.keys(e.answers).length),
    };
  // Copilot CLI: message + requestedSchema (options as `enum`), free-text answer; the merge
  // decision is a second field named `drwil-merge`, answered as `fiche=…, drwil-merge=…`.
  if (typeof e.message === "string" && e.message.startsWith(MARK)) {
    const entries = Object.entries(e.requestedSchema?.properties ?? {});
    const properties = entries.map(([, p]) => p);
    const fieldKey = entries.find(([k, p]) => k !== MERGE_MARK && Array.isArray(p?.enum))?.[0] ?? null;
    const options = entries.find(([k]) => k === fieldKey)?.[1]?.enum ?? [];
    const keys = entries.map(([k]) => k);
    const mergeKey = keys.includes(MERGE_MARK) ? MERGE_MARK : null;
    return { form: "copilot", options, fieldKey, mergeKey, keys, prefilled: properties.some((p) => p && "default" in p) };
  }
  return null;
}

/** The human's answer, read after the poll; null if missing. */
export function humanAnswer(input, poll) {
  if (poll.form === "claude") return input?.tool_response?.answers?.[poll.question] ?? null;
  const text = input?.tool_result?.text_result_for_llm;
  const m = typeof text === "string" ? /User responded:\s*(.+)\s*$/s.exec(text) : null;
  if (!m) return null;
  // One field: the whole answer. Several: `key=value, key=value`, in the schema's order of keys or not.
  return poll.mergeKey ? copilotField(m[1].trim(), poll.keys, poll.fieldKey) : m[1].trim();
}

/** The value of `key` in a Copilot CLI answer `key=value, key=value`; null if absent. */
function copilotField(answer, keys, key) {
  const esc = (k) => k.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const m = new RegExp(`(?:^|,\\s*)${esc(key ?? "")}=(.*?)(?=,\\s*(?:${keys.map(esc).join("|")})=|$)`, "s").exec(answer);
  return m ? m[1].trim() || null : null;
}

/**
 * The chosen labels in the order of the poll's options (QUA-037): Claude Code gives a multiple
 * choice in the order of the clicks. A label typed under "Other" keeps its typed place, after them.
 */
export function inPollOrder(choices, options = []) {
  const rank = (c) => {
    const i = options.indexOf(c);
    return i === -1 ? options.length : i;
  };
  return choices
    .map((c, i) => ({ c, i }))
    .sort((a, b) => rank(a.c) - rank(b.c) || a.i - b.i)
    .map(({ c }) => c);
}

/** Automatic merge authorised by the human answer to the merge question (second question or field). */
export function mergeAuthorised(input, poll) {
  if (poll.form === "claude") {
    if (!poll.mergeQuestion) return false;
    return MERGE_YES_RE.test(String(input?.tool_response?.answers?.[poll.mergeQuestion] ?? ""));
  }
  if (!poll.mergeKey) return false;
  const text = input?.tool_result?.text_result_for_llm;
  const m = typeof text === "string" ? /User responded:\s*(.+)\s*$/s.exec(text) : null;
  return MERGE_YES_RE.test(String(m ? (copilotField(m[1].trim(), poll.keys, poll.mergeKey) ?? "") : ""));
}

function language() {
  try {
    return JSON.parse(readFileSync(join(ROOT, ".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr";
  } catch {
    return "fr";
  }
}

async function main() {
  let input;
  try {
    input = JSON.parse(readFileSync(0, "utf8"));
  } catch {
    return 0;
  }
  const poll = startPoll(input);
  if (!poll) return 0;
  const event = input.hook_event_name;
  if (event === "PreToolUse") {
    if (!poll.prefilled) return 0;
    // Exit code 2: the tool blocks the call and gives this message back to the model.
    console.error(TEXTS[language()].refused);
    return 2;
  }
  if (event !== "PostToolUse") return 0;
  const T = TEXTS[language()];
  const choice = humanAnswer(input, poll);
  if (!choice) {
    tell(`✗ ${T.noAnswer}`);
    return 0;
  }
  // Several briefs (multiple choice, labels separated by commas; a brief path never holds one): a queue.
  const chosen = String(choice)
    .split(/,\s*/)
    .map((c) => c.trim())
    .filter(Boolean);
  const [first, ...queue] = inPollOrder(chosen, poll.options);
  if ([first, ...queue].some((c) => NOT_NOW_RE.test(c))) {
    tell(T.notNow);
    return 0;
  }
  try {
    const { start } = await import(pathToFileURL(join(ROOT, ".githooks", "state.mjs")).href);
    const r = start(ROOT, first, { human: true, autoMerge: mergeAuthorised(input, poll), queue });
    tell(`${r.code === 0 ? "✓" : "✗"} ${r.message}`);
  } catch (e) {
    // Never blocks the tool: the state stays unchanged, the Git gate remains; the agent is told why.
    tell(`✗ ${T.error(e?.message ?? String(e))}`);
  }
  return 0;
}

// Real paths on both sides: a project path through a symbolic link (macOS /var → /private/var) must
// still run the hook, not leave it silently idle.
if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  try {
    process.exitCode = await main();
  } catch {
    // An error never blocks the tool: the state stays unchanged, the Git gate remains.
    process.exitCode = 0;
  }
}
