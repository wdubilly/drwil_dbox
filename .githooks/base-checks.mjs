#!/usr/bin/env node
// Replays the checks with the main branch's hooks (QUA-113). In CI, a pull/merge request runs the
// scripts of the branch it checks: a PR that weakens `.githooks/` or removes a check from
// `.drwil/ia-first.json` would validate itself. Here the hooks are read from the main branch through
// Git (never a second clone), laid over the PR's working tree for the time of one run of
// `run-checks.mjs`, then restored; the list of checks stays the PR's. A check removed from that list,
// or a hook file kept at its PR version, must be cited in backticks at the head of a bullet of the
// « Contrats modifiés » section of a brief of the PR (approved by the requester).
// Self-contained (Node built-ins only): the CI runs the main branch's copy of this very file.
// QUA-013: a main branch that cannot be found or read is "not run", never a success.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync, realpathSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

// Messages with `{0}` filled by `say` (strings, not functions: each one stays covered by any test).
const TEXTS = {
  fr: {
    noBase:
      "non exécuté : branche principale inconnue (--base, DRWIL_BASE, GITHUB_BASE_REF, CI_MERGE_REQUEST_TARGET_BRANCH_NAME ou CI_DEFAULT_BRANCH)",
    unknownBase:
      "non exécuté : branche principale « {0} » introuvable dans le dépôt (premier push, branche orpheline, clone superficiel ?)",
    unreadable: "non exécuté : {0}",
    noHooks: "non applicable : « {0} » n'a pas encore de .githooks/ (adoption du kit), rien à rejouer",
    skipped:
      "rejeu sauté : .githooks/ et .drwil/ia-first.json sont identiques à ceux de « {0} », le rejeu serait strictement le passage déjà fait",
    kept: "{0} : version de la PR gardée, déclarée dans « Contrats modifiés »",
    removed:
      "contrôle « {0} » retiré de .drwil/ia-first.json sans déclaration : le citer entre backticks dans la section « Contrats modifiés » de la fiche, validée par le demandeur",
    failed: "les contrôles échouent avec les hooks de « {0} » : un hook affaibli ou contourné par la PR ?",
    ok: "contrôles rejoués avec les hooks de « {0} » : OK",
    undeclared:
      "`{0}` modifié par la branche sans être déclaré : en CI, le rejeu par la branche principale (QUA-113) remettrait sa version et ferait échouer les contrôles — ajouter à la section « Contrats modifiés » de la fiche la puce « - `{0}` — <ce qui change> (validé par le demandeur le <date>) » ; déclarer un hook change les règles du jeu : la puce s'écrit sur le oui du demandeur, jamais sans",
    hooksNotRun:
      "hooks non déclarés : non exécuté — {0} : les fichiers de .githooks/ modifiés par la branche ne sont pas comparés à « Contrats modifiés », ce n'est pas un succès",
    hooksNoBase: "branche principale introuvable (DRWIL_BASE, origin/HEAD, origin/master, origin/main, master ou main)",
  },
  en: {
    noBase: "not run: unknown main branch (--base, DRWIL_BASE, GITHUB_BASE_REF, CI_MERGE_REQUEST_TARGET_BRANCH_NAME or CI_DEFAULT_BRANCH)",
    unknownBase: 'not run: main branch "{0}" not found in the repository (first push, orphan branch, shallow clone?)',
    unreadable: "not run: {0}",
    noHooks: 'not applicable: "{0}" has no .githooks/ yet (kit adoption), nothing to replay',
    skipped:
      'replay skipped: .githooks/ and .drwil/ia-first.json are identical to those of "{0}", the replay would be strictly the run already done',
    kept: "{0}: PR version kept, declared in « Modified contracts »",
    removed:
      'check "{0}" removed from .drwil/ia-first.json without declaration: cite it in backticks in the « Modified contracts » section of the brief, approved by the requester',
    failed: 'the checks fail with the hooks of "{0}": a hook weakened or bypassed by the PR?',
    ok: 'checks replayed with the hooks of "{0}": OK',
    undeclared:
      "`{0}` modified by the branch without being declared: in CI, the replay by the main branch (QUA-113) would put back its version and make the checks fail — add to the « Modified contracts » section of the brief the bullet « - `{0}` — <what changes> (approved by the requester on <date>) »; declaring a hook changes the rules: the bullet is written on the requester's yes, never without",
    hooksNotRun:
      "undeclared hooks: not run — {0}: the .githooks/ files modified by the branch are not compared to « Modified contracts », this is not a success",
    hooksNoBase: "main branch not found (DRWIL_BASE, origin/HEAD, origin/master, origin/main, master or main)",
  },
};
const say = (template, value) => template.replaceAll("{0}", () => value);

const SECTION = /^##\s*(\d+\.\s*)?(Contrats modifiés|Modified contracts)\b/i;
const BULLET = /^\s*-\s+`([^`\s]+)`/;

/** Output of a git command; null when `maybe` and it fails, otherwise a failure throws (QUA-013). */
function git(root, args, maybe = false) {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
  if (r.status === 0) return r.stdout;
  if (maybe) return null;
  throw new Error(`git ${args.join(" ")} : ${String(r.stderr).trim()}`);
}

/** Main branch to read the hooks from: `--base <ref>`, DRWIL_BASE, then the forge's variables. */
export function baseRef(argv, env) {
  const i = argv.indexOf("--base");
  if (i >= 0) return argv[i + 1] || null;
  if (env.DRWIL_BASE) return env.DRWIL_BASE;
  const branch = env.GITHUB_BASE_REF || env.CI_MERGE_REQUEST_TARGET_BRANCH_NAME || env.CI_DEFAULT_BRANCH;
  return branch ? `origin/${branch}` : null;
}

/** Tokens in backticks at the head of the bullets of the « Contrats modifiés » sections of `text`. */
export function declaredIn(text) {
  const out = [];
  let inside = false;
  for (const line of text.split(/\r?\n/)) {
    if (/^##\s/.test(line)) inside = SECTION.test(line);
    else if (inside && BULLET.test(line)) out.push(BULLET.exec(line)[1]);
  }
  return out;
}

/**
 * What the briefs touched by the PR (commits `base..HEAD`) declare, each read at its last version:
 * the working tree, or the parent of the commit that deleted it (a closed workstream).
 */
export function declarations(root, base, briefsDir) {
  const out = new Set();
  const log = git(root, ["log", "--no-renames", "--format=%H", "--name-status", `${base}..HEAD`, "--", briefsDir]);
  const seen = new Set();
  let commit = "";
  for (const line of log.split("\n").filter(Boolean)) {
    const [status, file] = line.split("\t");
    if (file === undefined) commit = status;
    else if (!seen.has(file) && file.endsWith(".md")) {
      seen.add(file);
      const text = git(root, ["show", status === "D" ? `${commit}^:${file}` : `${commit}:${file}`]);
      for (const token of declaredIn(text)) out.add(token);
    }
  }
  return out;
}

/**
 * QUA-137: files of `.githooks/` the branch modifies or deletes (working tree against its merge base
 * with the main branch) without declaring them, asked by the closing before the CI's replay: the replay would put them back at the main branch's version.
 * `text`: the active brief as it stands (a declaration not committed yet counts). `{ files, lines }`,
 * one message per file; no main branch or git failing: no file and one "not run" line, never a
 * silent success (QUA-013); no `.githooks/` at all: nothing to compare.
 */
export function undeclaredHooks(root, text = "", { env = process.env } = {}) {
  let T = TEXTS.fr;
  if (!existsSync(join(root, ".githooks"))) return { files: [], lines: [] };
  try {
    const file = join(root, ".drwil", "ia-first.json");
    const cfg = existsSync(file) ? JSON.parse(readFileSync(file, "utf8")) : {};
    T = TEXTS[cfg.lang === "en" ? "en" : "fr"];
    const origin = git(root, ["symbolic-ref", "--quiet", "refs/remotes/origin/HEAD"], true)?.trim();
    const refs = [env.DRWIL_BASE, origin, "origin/master", "origin/main", "master", "main"];
    const ref = refs.find((r) => r && git(root, ["rev-parse", "--verify", "--quiet", `${r}^{commit}`], true) !== null);
    if (!ref) return { files: [], lines: [say(T.hooksNotRun, T.hooksNoBase)] };
    const base = git(root, ["merge-base", "HEAD", ref]).trim();
    const declared = declarations(root, base, cfg.dirs?.projects ?? "docs/projects");
    for (const token of declaredIn(text)) declared.add(token);
    const changed = git(root, ["diff", "--no-renames", "--name-only", "--diff-filter=MD", base, "--", ".githooks/"]);
    const isDeclared = (f) => declared.has(f) || declared.has(f.replace(/\.test\.mjs$/, ".mjs"));
    const files = changed.split("\n").filter((f) => f && !isDeclared(f));
    return { files, lines: files.map((f) => say(T.undeclared, f)) };
  } catch (e) {
    return { files: [], lines: [say(T.hooksNotRun, e.message)] };
  }
}

const keys = (cfg) => (Array.isArray(cfg.checks) ? cfg.checks : []).map((c) => String(c.id ?? c.name ?? c.run));

function runChecks(root, env, stdio) {
  return spawnSync(process.execPath, [join(".githooks", "run-checks.mjs")], { cwd: root, env, stdio }).status;
}

/**
 * True only when `.githooks/` and the config are byte for byte the base's (tracked or not): the replay
 * would then be the very run already done. Any doubt (a failing git, a difference) means "replay".
 */
function unchanged(root, base) {
  const paths = [".githooks/", ".drwil/ia-first.json"];
  const diff = git(root, ["diff", "--quiet", base, "--", ...paths], true);
  const extra = git(root, ["ls-files", "--others", "--exclude-standard", "--", ...paths], true);
  return diff === "" && extra?.trim() === "";
}

/** Replays the checks; `{ status: "ok" | "skipped" | "fail" | "not-run" | "not-applicable", lines }`. */
export function replay(root, { argv = [], env = process.env, stdio = "inherit" } = {}) {
  let T = TEXTS.fr;
  try {
    const cfg = JSON.parse(readFileSync(join(root, ".drwil", "ia-first.json"), "utf8"));
    T = TEXTS[cfg.lang === "en" ? "en" : "fr"];
    const ref = baseRef(argv, env);
    if (!ref) return { status: "not-run", lines: [T.noBase] };
    const found = git(root, ["rev-parse", "--verify", "--quiet", `${ref}^{commit}`], true);
    if (found === null) return { status: "not-run", lines: [say(T.unknownBase, ref)] };
    const base = found.trim();
    const hooks = git(root, ["ls-tree", "-r", "-z", "--name-only", base, "--", ".githooks/"]).split("\0").filter(Boolean);
    if (!hooks.length) return { status: "not-applicable", lines: [say(T.noHooks, ref)] };
    if (unchanged(root, base)) return { status: "skipped", lines: [say(T.skipped, ref)] };
    const declared = declarations(root, base, cfg.dirs?.projects ?? "docs/projects");
    const baseCfg = git(root, ["show", `${base}:.drwil/ia-first.json`], true);
    const now = new Set(keys(cfg));
    const removed = keys(JSON.parse(baseCfg ?? "{}")).filter((k) => !now.has(k) && !declared.has(k));
    const kept = hooks.filter((f) => declared.has(f) || declared.has(f.replace(/\.test\.mjs$/, ".mjs")));
    const status = overlay(
      root,
      base,
      hooks.filter((f) => !kept.includes(f)),
      () => runChecks(root, env, stdio),
    );
    const lines = [...kept.map((f) => say(T.kept, f)), ...removed.map((k) => say(T.removed, k)), say(status === 0 ? T.ok : T.failed, ref)];
    return { status: status === 0 && !removed.length ? "ok" : "fail", lines };
  } catch (e) {
    return { status: "not-run", lines: [say(T.unreadable, e.message)] };
  }
}

/**
 * Lays the main branch's version of `files` over the working tree for the time of `run`, then puts
 * back the PR's (a file the PR deleted is removed again). Everything is read through Git into a
 * temporary folder first: a failed read leaves the working tree untouched.
 */
function overlay(root, base, files, run) {
  const dir = mkdtempSync(join(tmpdir(), "drwil-base-checks-"));
  const saved = new Map();
  try {
    for (const f of files) {
      mkdirSync(dirname(join(dir, f)), { recursive: true });
      writeFileSync(join(dir, f), git(root, ["show", `${base}:${f}`]));
    }
    for (const f of files) {
      const abs = join(root, f);
      saved.set(abs, existsSync(abs) ? readFileSync(abs) : null);
      mkdirSync(dirname(abs), { recursive: true });
      writeFileSync(abs, readFileSync(join(dir, f)));
    }
    return run();
  } finally {
    for (const [abs, before] of saved) before === null ? rmSync(abs) : writeFileSync(abs, before);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (process.argv[1] && realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))) {
  const r = replay(process.cwd(), { argv: process.argv.slice(2) });
  for (const line of r.lines) console.log(`[ia-first] ${line}`);
  process.exitCode = ["ok", "skipped", "not-applicable"].includes(r.status) ? 0 : 1;
}
