// Single check engine:
// .githooks/run-checks.mjs (git adapter) and `drwil verify` both use it,
// neither reimplements a check. Dependency-free: the hooks stay self-contained.
//
// Each check has a stable identifier (cited by the **Check** field of
// docs/contracts.md) and returns one of these results (stable values: they leave the hooks
// through `drwil verify` and its evidence):
// - "ok" / "fail";
// - "not-run": it should have run but could not (missing tool, deferred
//   at commit time) — never a success (QUA-013);
// - "not-applicable": excluded by design in this context (e.g. QUA-017 in CI).
import { spawnSync } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { globToRegex } from "./glob.mjs";
import { checkScope } from "./scope-gate.mjs";
import { normalize } from "./scope.mjs";

const TEXTS = {
  fr: {
    secrets: "secrets (gitleaks)",
    gitleaksMissing: "gitleaks absent",
    noGit: "pas de dépôt git",
    minimalMode: "mode minimal",
    docs: "chemins et contrats cités dans la doc",
    contractTests: "tests cités par les contrats, présents et porteurs de leur ID (QUA-029)",
    architecture: "carte d'architecture à jour (QUA-040)",
    scope: "périmètre de l'attente",
    branch: "branche principale (QUA-017)",
    branchCi: "jamais en CI",
    branchTags: "push de tags uniquement",
    branchNoBranch: "push de tags ou de suppressions de branches uniquement",
    branchProtected: (b) =>
      `travail direct sur la branche principale (${b}) : passer par une branche et une pull/merge request (voir docs/recipes/working-with-branches.md)`,
    selfTests: "tests des contrôles eux-mêmes",
    noTests: "tests propres au dépôt drwil, non livrés",
    ciCoverage: "couverture CI de chaque contrôle (QUA-013)",
    noCi: "aucune CI configurée (ci: none) : pas de filet pour les contrôles dégradables — en déclarer une : `npx drwil init --ci github` (ou `--ci gitlab`)",
    deferred: "aucun fichier indexé sous ses chemins : rejoué au push et en CI",
    docsOnly: "commit de documentation seule : rejoué au push et en CI",
  },
  en: {
    secrets: "secrets (gitleaks)",
    gitleaksMissing: "gitleaks not installed",
    noGit: "not a git repository",
    minimalMode: "minimal mode",
    docs: "paths and contracts cited in docs",
    contractTests: "tests cited by the contracts, present and carrying their ID (QUA-029)",
    architecture: "architecture map up to date (QUA-040)",
    scope: "brief scope",
    branch: "main branch (QUA-017)",
    branchCi: "never in CI",
    branchTags: "tags-only push",
    branchNoBranch: "push of tags or branch deletions only",
    branchProtected: (b) =>
      `direct work on the main branch (${b}): go through a branch and a pull/merge request (see docs/recipes/working-with-branches.md)`,
    selfTests: "tests of the checks themselves",
    noTests: "tests owned by the drwil repository, not shipped",
    ciCoverage: "CI coverage of each check (QUA-013)",
    noCi: "no CI configured (ci: none): no net for degradable checks — declare one: `npx drwil init --ci github` (or `--ci gitlab`)",
    deferred: "no staged file under its paths: replayed on push and in CI",
    docsOnly: "documentation-only commit: replayed on push and in CI",
  },
};

/**
 * Project root for run-checks.mjs (`scriptUrl`: its import.meta.url): the parent folder of
 * `.githooks/`, whatever the current folder. `DRWIL_ROOT` replaces it, for its tests only:
 * without it, run-checks would only be tried on the drwil repository itself.
 */
export function projectRoot(scriptUrl, env = process.env) {
  return env.DRWIL_ROOT || join(dirname(fileURLToPath(scriptUrl)), "..");
}

export function texts(lang) {
  return TEXTS[lang === "en" ? "en" : "fr"];
}

function findGitleaks() {
  for (const candidate of ["gitleaks", join(homedir(), ".local", "bin", "gitleaks")]) {
    if (!spawnSync(candidate, ["version"], { stdio: "ignore" }).error) return candidate;
  }
  return null;
}

/**
 * The project's checks, in the hooks' run order: [{ id, name, run }].
 * `run({ stdio, beforeRun })` returns { status, detail?, message?, output? } (`message`:
 * explanation of a failure to show instead of the name); `beforeRun` is called just before
 * actually launching the command (not for a skipped check).
 * `staged` (pre-commit hook only): staged files, to defer a project check outside its `paths`;
 * null everywhere else (nothing is deferred).
 * `whole`: analyses the whole repository rather than the staged files only (CI, `drwil verify`).
 */
export function checks(root, { cfg = {}, env = process.env, staged = null, whole = Boolean(env.CI) } = {}) {
  const T = texts(cfg.lang);
  const full = (cfg.mode ?? "full") === "full";
  const launch = (cmd, args, { stdio = "inherit", beforeRun, keepGit = false, ...opts } = {}, name) => {
    beforeRun?.(name);
    // NODE_TEST_CONTEXT propagates to children: if the engine itself runs under `node --test`
    // (the kit testing its hooks, a harness calling verify…), a `node --test` launched by a check
    // would be silently skipped ("called recursively") and return 0: a false PASS.
    const checkEnv = { ...(opts.env ?? env), NODE_TEST_CONTEXT: undefined };
    // QUA-057: a hook run from a worktree or by `git commit -a` receives GIT_DIR, GIT_INDEX_FILE…;
    // a check that inherits them and runs `git init` elsewhere acts on the repository that launched it
    // (core.bare=true written in the shared config, index soiled). The hook keeps them; its children
    // run from the project root, where git finds the right repository on its own.
    if (!keepGit) for (const k of Object.keys(checkEnv)) if (k.startsWith("GIT_")) delete checkEnv[k];
    const r = spawnSync(cmd, args, { cwd: root, stdio, encoding: "utf8", ...opts, env: checkEnv });
    const output = stdio === "pipe" ? `${r.stdout ?? ""}${r.stderr ?? ""}` : undefined;
    // A command that cannot even start is a failure, as before the engine was extracted.
    if (r.error) return { status: "fail", detail: r.error.message, output };
    return { status: r.status === 0 ? "ok" : "fail", output };
  };
  const list = [];
  const add = (id, name, run) => list.push({ id, name, run: (o = {}) => run(o, name) });
  // A commit that stages documentation only (a brief widened mid-workstream) does not replay the tests
  // on the working tree, which holds the lot in progress (QUA-035); push and CI always replay them.
  // Strictly docs/ or Markdown: not isCode(), which calls code only what the project declares.
  const docsOnly = Boolean(staged?.length) && staged.every((f) => /^docs\//.test(normalize(f)) || /\.md$/i.test(f));

  add("secrets", T.secrets, (o, name) => {
    if (!full) return { status: "not-applicable", detail: T.minimalMode };
    const gitleaks = findGitleaks();
    if (!gitleaks) return { status: "not-run", detail: T.gitleaksMissing };
    if (spawnSync("git", ["rev-parse", "--git-dir"], { cwd: root, stdio: "ignore" }).status !== 0)
      return { status: "not-run", detail: T.noGit, configured: true };
    // In CI (or for `drwil verify`) nothing is staged to judge: the whole history is analysed,
    // otherwise a proof of "no secret in the repository" would look at nothing.
    const args = whole ? ["git", "--no-banner", "--redact", "."] : ["git", "--pre-commit", "--staged", "--no-banner", "--redact", "."];
    // Only reads the repository, and must see the index of the commit in progress: under
    // `git commit -a` it is the one named by GIT_INDEX_FILE, not .git/index.
    return launch(gitleaks, args, { ...o, keepGit: true }, name);
  });

  add("docs-references", T.docs, (o, name) => launch(process.execPath, [".githooks/check-docs.mjs"], o, name));
  add("contract-tests", T.contractTests, (o, name) => launch(process.execPath, [".githooks/check-contract-tests.mjs"], o, name));
  add("architecture-map", T.architecture, (o, name) => launch(process.execPath, [".githooks/check-architecture.mjs"], o, name));

  // Brief scope gate (.githooks/scope-gate.mjs): at commit time, the local state;
  // elsewhere (push, CI, verify), each commit of the branch is replayed through its trailer.
  add("scope-gate", T.scope, (o, name) => {
    o.beforeRun?.(name);
    const r = checkScope(root, { cfg, staged, env });
    const output = r.problems.map((p) => `  ${r.status === "fail" ? "✗" : "⚠"} ${p}`).join("\n");
    if (output && o.stdio !== "pipe") console.log(output);
    return { status: r.status, message: r.message, detail: r.detail, configured: r.configured, output };
  });

  // QUA-017: never work directly on the main branch, including the very first commit (no more
  // bootstrap exception: `init()` always creates a working branch before any commit).
  // `symbolic-ref` (unlike `rev-parse --abbrev-ref HEAD`) works even before the first commit
  // (HEAD not created yet). Never in CI (env.CI): CI also runs on master after a legitimate merge,
  // which must not be blocked retroactively — only the local hooks (pre-commit/pre-push) apply it.
  // Never either on a push of tags only (DRWIL_PUSH_TAGS_ONLY, set by .githooks/pre-push from the
  // hook's standard input): a tag never changes a branch; nor on a remote branch deletion
  // (DRWIL_PUSH_NO_BRANCH).
  add("main-branch", T.branch, () => {
    if (env.CI) return { status: "not-applicable", detail: T.branchCi };
    if (env.DRWIL_PUSH_TAGS_ONLY === "1") return { status: "not-applicable", detail: T.branchTags };
    if (env.DRWIL_PUSH_NO_BRANCH === "1") return { status: "not-applicable", detail: T.branchNoBranch };
    const branch = spawnSync("git", ["symbolic-ref", "--short", "HEAD"], { cwd: root, encoding: "utf8" });
    const branchName = branch.status === 0 ? branch.stdout.trim() : null;
    if (branchName && ["master", "main"].includes(branchName)) return { status: "fail", message: T.branchProtected(branchName) };
    return { status: "ok" };
  });

  add("checks-self-test", T.selfTests, (o, name) => {
    if (!full) return { status: "not-applicable", detail: T.minimalMode };
    const dir = join(root, ".githooks");
    const tests = existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith(".test.mjs")) : [];
    // These tests try drwil's code: they stay in its repository and do not slow down the commits
    // of an equipped project, which does not receive them.
    if (!tests.length) return { status: "not-applicable", detail: T.noTests };
    if (docsOnly) return { status: "not-run", detail: T.docsOnly };
    return launch(process.execPath, ["--test", ...tests.map((f) => join(".githooks", f))], o, name);
  });

  add("ci-coverage", T.ciCoverage, (o, name) => {
    if (!full) return { status: "not-applicable", detail: T.minimalMode };
    const ciFiles = Array.isArray(cfg.ciFiles) ? cfg.ciFiles : [];
    if (!ciFiles.length) return { status: "not-run", detail: T.noCi, configured: true };
    return launch(process.execPath, [".githooks/check-control-coverage.mjs"], o, name);
  });

  // Stack checks (lint, typecheck, tests) are declared by the project, not by the kit.
  // At pre-commit only (`staged` given), a check that declares `paths` is deferred if no staged
  // file matches them: a docs commit does not rerun the whole test suite. It stays "not run"
  // (QUA-013) and the pre-push as well as CI always replay it.
  for (const c of Array.isArray(cfg.checks) ? cfg.checks : []) {
    const name = c.name ?? c.run;
    list.push({
      id: c.id ?? `project:${name}`,
      name,
      run: (o = {}) => {
        if (docsOnly) return { status: "not-run", detail: T.docsOnly };
        if (staged && Array.isArray(c.paths) && c.paths.length) {
          const patterns = c.paths.map(globToRegex);
          if (!staged.some((f) => patterns.some((p) => p.test(f)))) return { status: "not-run", detail: T.deferred };
        }
        return launch(c.run, [], { ...o, shell: true, cwd: c.cwd ? join(root, c.cwd) : root }, name);
      },
    });
  }
  return list;
}
