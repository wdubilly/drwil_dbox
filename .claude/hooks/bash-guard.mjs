#!/usr/bin/env node
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL } from "node:url";

// Guard of Claude Code's shell commands (PreToolUse hook, Bash matcher).
//
// A net specific to the tool, on top of the git checks (docs/ia-first.md, section 4): the hook
// sees the raw command, which permission rules only see split. It blocks nothing: on a risky
// command, it makes Claude ask the person for approval ("ask") with the reason, then they decide.
//
// Deliberately narrow, so as not to wear people out: secret file names (SEC-007), not words
// (`process.env` passes), and piping into a shell.

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

const TEXTS = {
  fr: {
    env: "fichier .env (secrets) : utiliser .env.example, versionné et sans secret",
    ssh: "clé SSH privée",
    key: "fichier de clé ou de certificat",
    credentials: "fichier d'identifiants",
    pipe: "pipe vers un shell (| sh, | bash)",
    reason: (r) => `garde-fou : accord demandé — ${r}`,
  },
  en: {
    env: ".env file (secrets): use .env.example, versioned and secret-free",
    ssh: "private SSH key",
    key: "key or certificate file",
    credentials: "credentials file",
    pipe: "pipe into a shell (| sh, | bash)",
    reason: (r) => `guard: approval requested — ${r}`,
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
  const found = reason(String(event.tool_input?.command ?? ""), T);
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

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  let input = "";
  process.stdin.on("data", (d) => {
    input += d;
  });
  process.stdin.on("end", () => {
    const out = main(input);
    if (out) console.log(out);
  });
}
