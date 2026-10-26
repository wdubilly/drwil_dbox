#!/usr/bin/env node
import { readFileSync, realpathSync } from "node:fs";
import { dirname, isAbsolute, join } from "node:path";
import { fileURLToPath } from "node:url";

// Guard of Claude Code's shell commands (PreToolUse hook, Bash matcher).
//
// A net specific to the tool, on top of the git checks (docs/ia-first.md, section 4): the hook
// sees the raw command, which permission rules only see split. On a risky command, it makes
// Claude ask the person for approval ("ask") with the reason, then they decide.
//
// Deliberately narrow, so as not to wear people out: secret file names (SEC-007), not words
// (`process.env` passes), and piping into a shell.
//
// One family is refused outright, never asked (QUA-114): skipping the git hooks or writing,
// removing or overriding core.hooksPath (--no-verify, commit -n, -c core.hooksPath=,
// git config core.hooksPath …, GIT_CONFIG_*), also inside a compound command or a `sh -c "…"`.
// A blocking check is fixed, never bypassed (QUA-019); reading the setting stays allowed.
//
// Also refused: `cat`, `less` or `more` of the contract registry or the journal, read whole (QUA-132,
// the shell side of .claude/hooks/read-guard.mjs). Judged on the text of the command, like the rest:
// `sed -n`, `grep`, `head` stay allowed, and a reading hidden behind a variable is not seen.

// The folder holding this hook's .claude/, never the current folder (an agent's `cd`, QUA-044).
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
// Optional: without the read guard (a partial copy), this guard keeps its other refusals.
let readGuard = null;
try {
  readGuard = await import("./read-guard.mjs");
} catch {}

// Left edge: no letter, no `$`, no `\` (a `\.env` regex is not a file).
const BEFORE = "(?<![\\w$\\\\])";
const RISKS = [
  // .env and .env.<suffix>, but not .env.example nor .env.template (versioned).
  [new RegExp(`${BEFORE}\\.env(?:\\.(?!example\\b|template\\b)[\\w-]+)?(?![\\w.-])`), "env"],
  // Private SSH key, full name (id_rsa_x included), not the public key.
  [/\bid_(?:rsa|ed25519|ecdsa|dsa)[\w-]*(?![\w-]|\.pub)/, "ssh"],
  [/[\w-]\.(?:pem|p12|pfx|jks)(?![\w.])/, "key"],
  [new RegExp(`${BEFORE}\\.(?:netrc|pgpass)(?![\\w.])`), "credentials"],
  [/\|\s*(?:sudo\s+)?(?:ba|z|da)?sh(?:\s|$)/, "pipe"],
];

// git options that take the next word as their value (before the subcommand, then for commit).
const GIT_VALUED = new Set(["-C", "-c", "--git-dir", "--work-tree", "--namespace", "--config-env", "--super-prefix"]);
const COMMIT_VALUED = new Set(["-m", "-F", "-c", "-C", "-t", "--message", "--file", "--author", "--date", "--fixup", "--squash"]);
const CONFIG_WRITE = new Set([
  "--unset",
  "--unset-all",
  "--add",
  "--replace-all",
  "--remove-section",
  "--rename-section",
  "--edit",
  "-e",
  "set",
  "unset",
  "edit",
  "remove-section",
  "rename-section",
]);
const CONFIG_READ = new Set(["--get", "--get-all", "--get-regexp", "--get-urlmatch", "--list", "-l", "get", "list"]);
const CONFIG_VALUED = new Set(["--file", "-f", "--blob", "--type", "--default", "--comment"]);
const HOOKS_KEY = /^core\.hookspath$/i;
const HOOKS_OVERRIDE = /^(?:--config-env=)?core\.hookspath=/i;

/**
 * Commands of a shell text, each one a list of words (quotes removed), and the quoted strings,
 * analysed again as commands (`sh -c "…"`). A linear scan, no regular expression on the whole text.
 * `literal`: a `\` stays in the word, quoted or not (a Windows path, `C:\Users\…`).
 */
function split(text, literal = false) {
  const commands = [[]];
  const quoted = [];
  let word = null;
  let quote = null;
  let inner = "";
  const end = () => {
    if (word !== null) commands[commands.length - 1].push(word);
    word = null;
  };
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (quote !== null) {
      if (ch === quote) {
        quote = null;
        quoted.push(inner);
        continue;
      }
      const escaped = ch === "\\" && !literal && quote === '"' && i + 1 < text.length;
      const add = escaped ? text[++i] : ch;
      word += add;
      inner += add;
    } else if (ch === '"' || ch === "'") {
      quote = ch;
      inner = "";
      word = word ?? "";
    } else if (ch === "\\" && !literal && i + 1 < text.length) word = (word ?? "") + text[++i];
    else if (" \t\r".includes(ch)) end();
    else if (";&|\n()`".includes(ch)) {
      end();
      commands.push([]);
    } else word = (word ?? "") + ch;
  }
  end();
  return { commands: commands.filter((c) => c.length), quoted };
}

/** A `git commit` short-flag cluster that holds -n (--no-verify), before any flag taking a value. */
function clusterSkips(arg) {
  if (!/^-[a-zA-Z]+$/.test(arg)) return false;
  for (const letter of arg.slice(1)) {
    if (letter === "n") return true;
    if ("mFcCt".includes(letter)) return false;
  }
  return false;
}

/** Whether one command (its words) skips the git hooks or writes core.hooksPath. */
function commandSkips(words) {
  const at = words.findIndex((w) => /(^|\/)git$/.test(w));
  if (at < 0) return false;
  let i = at + 1;
  for (; i < words.length && words[i].startsWith("-"); i++) {
    if (HOOKS_OVERRIDE.test(words[i]) || HOOKS_OVERRIDE.test(words[i].replace(/^-c/, ""))) return true;
    if (GIT_VALUED.has(words[i]) && HOOKS_OVERRIDE.test(words[i + 1] ?? "")) return true;
    if (GIT_VALUED.has(words[i])) i++;
  }
  const sub = words[i];
  const args = words.slice(i + 1);
  if (args.some((a) => /^--no-veri(?:fy?)?$/.test(a))) return true;
  if (sub === "commit") {
    for (let k = 0; k < args.length && args[k] !== "--"; k++) {
      if (clusterSkips(args[k])) return true;
      if (COMMIT_VALUED.has(args[k])) k++;
    }
  }
  if (sub !== "config") return false;
  if (args.some((a) => CONFIG_WRITE.has(a)) && args.some((a) => HOOKS_KEY.test(a) || a === "core")) return true;
  if (args.some((a) => CONFIG_READ.has(a))) return false;
  const positional = args.filter((a, k) => !a.startsWith("-") && !CONFIG_VALUED.has(args[k - 1]));
  const key = positional.findIndex((a) => HOOKS_KEY.test(a));
  return key >= 0 && positional.length > key + 1;
}

/**
 * Whether a shell command skips the git hooks or writes, removes or overrides core.hooksPath
 * (QUA-114): any command of the text, quoted strings included (`sh -c "…"`, `eval '…'`).
 */
export function skipsHooks(command, depth = 0) {
  if (depth > 3) return false;
  const text = String(command);
  const mentionsHooks = /hookspath/i.test(text);
  // GIT_CONFIG_COUNT/KEY_<n>/VALUE_<n>/PARAMETERS… set a configuration for one command.
  if (mentionsHooks && /\bGIT_CONFIG_\w*=/.test(text)) return true;
  if (mentionsHooks && /\.git[\\/]config\b/.test(text) && /\bsed\s+-i|\btee\b|>/.test(text)) return true;
  const { commands, quoted } = split(text);
  return commands.some(commandSkips) || quoted.some((q) => skipsHooks(q, depth + 1));
}

const WHOLE_READERS = new Set(["cat", "less", "more"]);

/**
 * The guarded file (registry or journal, QUA-132) that a shell command reads whole with `cat`, `less`
 * or `more`, or null. A relative path matches by its end (the agent may have `cd`'d), an absolute
 * one must be the project's file. The words are read twice, `\` as an escape (POSIX shell) and as
 * a character: an unquoted Windows path (`C:\Users\…`) would otherwise lose its separators.
 */
export function wholeRead(command, root = ROOT) {
  if (!readGuard) return null;
  const files = readGuard.guardedFiles(root);
  const target = (arg) => {
    if (isAbsolute(arg)) return readGuard.guardedTarget(arg, root);
    const a = arg.replace(/\\/g, "/").replace(/^\.\//, "");
    return files.find((rel) => a === rel || a.endsWith(`/${rel}`)) ?? null;
  };
  const text = String(command);
  for (const words of [...split(text).commands, ...split(text, true).commands]) {
    let i = 0;
    while (i < words.length && /^\w+=/.test(words[i])) i++;
    if (!WHOLE_READERS.has((words[i] ?? "").split("/").pop())) continue;
    for (const arg of words.slice(i + 1)) {
      const found = arg.startsWith("-") ? null : target(arg);
      if (found) return found;
    }
  }
  return null;
}

const TEXTS = {
  fr: {
    env: "fichier .env (secrets) : utiliser .env.example, versionné et sans secret",
    ssh: "clé SSH privée",
    key: "fichier de clé ou de certificat",
    credentials: "fichier d'identifiants",
    pipe: "pipe vers un shell (| sh, | bash)",
    reason: (r) => `garde-fou : accord demandé — ${r}`,
    refused:
      "garde-fou : refusé (QUA-114) — saut des hooks git ou écriture de core.hooksPath (--no-verify, commit -n, -c core.hooksPath=, git config core.hooksPath, GIT_CONFIG_*). Un contrôle qui bloque se corrige, il ne se contourne pas (QUA-019) ; lire le réglage reste permis.",
  },
  en: {
    env: ".env file (secrets): use .env.example, versioned and secret-free",
    ssh: "private SSH key",
    key: "key or certificate file",
    credentials: "credentials file",
    pipe: "pipe into a shell (| sh, | bash)",
    reason: (r) => `guard: approval requested — ${r}`,
    refused:
      "guard: refused (QUA-114) — skipping the git hooks or writing core.hooksPath (--no-verify, commit -n, -c core.hooksPath=, git config core.hooksPath, GIT_CONFIG_*). A blocking check is fixed, never bypassed (QUA-019); reading the setting stays allowed.",
  },
};

// The project's language; Claude Code runs the hook with the project folder as cwd.
function texts() {
  try {
    return TEXTS[JSON.parse(readFileSync(join(".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr"];
  } catch {
    return TEXTS.fr;
  }
}

function reason(command, T = texts()) {
  const found = RISKS.filter(([pattern]) => pattern.test(command)).map(([, key]) => T[key]);
  return found.length ? found.join(" ; ") : null;
}

/** JSON answer of the hook, or an empty string to let permissions decide. */
function main(input) {
  let event;
  try {
    event = JSON.parse(input);
  } catch {
    return "";
  }
  if (typeof event !== "object" || event === null || event.tool_name !== "Bash") return "";
  const T = texts();
  const command = String(event.tool_input?.command ?? "");
  if (skipsHooks(command))
    return JSON.stringify({
      hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: "deny", permissionDecisionReason: T.refused },
    });
  let whole = null;
  try {
    whole = wholeRead(command);
  } catch {}
  if (whole)
    return JSON.stringify({
      hookSpecificOutput: {
        hookEventName: "PreToolUse",
        permissionDecision: "deny",
        permissionDecisionReason: readGuard.refusal(whole, ROOT),
      },
    });
  const found = reason(command, T);
  if (found === null) return "";
  return JSON.stringify({
    hookSpecificOutput: {
      hookEventName: "PreToolUse",
      permissionDecision: "ask",
      permissionDecisionReason: T.reason(found),
    },
  });
}

export { reason, main };

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  let input = "";
  process.stdin.on("data", (d) => {
    input += d;
  });
  process.stdin.on("end", () => {
    const out = main(input);
    if (out) console.log(out);
  });
}
