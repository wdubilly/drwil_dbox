// Single check engine: .githooks/run-checks.mjs (git adapter) and `drwil verify` both use it, neither reimplements a check.
// Dependency-free: the hooks stay self-contained. Each check has a stable identifier (cited by the **Check** field of
// docs/contracts.md) and returns one of these results (stable values: they leave the hooks through `drwil verify` and its evidence):
// "ok" / "fail"; "not-run": it should have run but could not (missing tool, deferred at commit time), never a success (QUA-013);
// "not-applicable": excluded by design in this context (e.g. QUA-017 in CI).
import { spawn, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, renameSync, rmSync } from "node:fs";
import { availableParallelism, homedir, tmpdir } from "node:os";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { globToRegex } from "./glob.mjs";
import { controlFingerprints, treeFingerprint } from "./fingerprints.mjs";
import { checkScope } from "./scope-gate.mjs";
import { normalize } from "./scope.mjs";

export { controlFingerprints, treeFingerprint };

// One temporary root per pass (QUA-133): each check gets its own sub-folder as TMPDIR/TMP/TEMP, removed at the end of the pass; the
// failed checks' folders move to a fixed path, replaced at each failure. A pass dying before `finish` is cleaned by the `exit` handler.
const passes = new Set();
let exitHooked = false;
const hookExit = () => {
  if (exitHooked) return;
  exitHooked = true;
  process.on("exit", () => {
    for (const finish of [...passes]) finish();
  });
};
const removeTree = (dir) => {
  try {
    rmSync(dir, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
    return true;
  } catch {
    return false;
  }
};

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
    buildFailed: (n) => `${n} : la compilation du projet (build) est en échec`,
    buildMissing: (n) => `${n} déclare needsBuild sans « build » dans .drwil/ia-first.json`,
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
    buildFailed: (n) => `${n}: the project build failed`,
    buildMissing: (n) => `${n} declares needsBuild without a "build" in .drwil/ia-first.json`,
  },
};

/** Project root for run-checks.mjs (`scriptUrl`: its import.meta.url): the parent of `.githooks/`; `DRWIL_ROOT` replaces it, for its tests only. */
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
 * The project's checks, in the hooks' run order: [{ id, name, run, reuse }].
 * `run({ stdio, beforeRun })` returns { status, detail?, message?, output? } (`message`: explanation of a failure to show instead of
 * the name); `beforeRun` is called just before actually launching the command (not for a skipped check).
 * `staged` (pre-commit hook only): staged files, to defer a project check outside its `paths`; null everywhere else.
 * `whole`: analyses the whole repository rather than the staged files only (CI, `drwil verify`).
 */
export function checks(root, { cfg = {}, env = process.env, staged = null, whole = Boolean(env.CI) } = {}) {
  const T = texts(cfg.lang);
  const full = (cfg.mode ?? "full") === "full";
  // NODE_TEST_CONTEXT propagates to children: under `node --test`, a `node --test` launched by a check would be silently skipped
  // ("called recursively") and return 0: a false PASS. QUA-057: a hook run from a worktree or by `git commit -a` receives GIT_DIR,
  // GIT_INDEX_FILE…; a check inheriting them that runs `git init` elsewhere would soil the launching repository: children lose them.
  const base = env.TMPDIR || tmpdir();
  const failedPath = join(base, `drwil-checks-failed-${createHash("sha1").update(root).digest("hex").slice(0, 8)}`);
  let passRoot = null;
  let sequence = 0;
  const launches = [];
  const tempFor = (name) => {
    try {
      if (!passRoot) {
        passRoot = mkdtempSync(join(base, "drwil-checks-"));
        passes.add(finish);
        hookExit();
      }
      const dir = join(
        passRoot,
        `${++sequence}-${String(name)
          .replace(/[^\w.-]+/g, "_")
          .slice(0, 40)}`,
      );
      mkdirSync(dir);
      const temp = { name, dir, failed: false, leaked: 0 };
      launches.push(temp);
      return temp;
    } catch {
      return null; // no temporary root: the check keeps the inherited one, as before
    }
  };
  const settle = (temp, ok) => {
    if (!temp) return {};
    temp.failed = !ok;
    try {
      temp.leaked = readdirSync(temp.dir).length;
    } catch {}
    return temp.leaked ? { leaked: temp.leaked } : {};
  };
  // Ends the pass: counts what the checks left, keeps the failed ones' folders, removes the rest.
  function finish() {
    passes.delete(finish);
    const result = {
      kept: null,
      leaks: launches.filter((l) => l.leaked).map((l) => ({ name: l.name, count: l.leaked })),
      cleanupFailed: false,
    };
    if (!passRoot) return result;
    const failed = launches.filter((l) => l.failed);
    if (failed.length) {
      removeTree(failedPath);
      try {
        mkdirSync(failedPath);
        for (const l of failed) renameSync(l.dir, join(failedPath, basename(l.dir)));
        result.kept = failedPath;
      } catch {}
    }
    result.cleanupFailed = !removeTree(passRoot);
    passRoot = null;
    launches.length = 0;
    return result;
  }
  const prepare = (opts, keepGit, temp) => {
    const checkEnv = { ...(opts.env ?? env), NODE_TEST_CONTEXT: undefined };
    if (temp) checkEnv.TMPDIR = checkEnv.TMP = checkEnv.TEMP = temp.dir;
    if (!keepGit) for (const k of Object.keys(checkEnv)) if (k.startsWith("GIT_")) delete checkEnv[k];
    return { cwd: root, encoding: "utf8", ...opts, env: checkEnv };
  };
  const launch = (cmd, args, { stdio = "inherit", beforeRun, keepGit = false, ...opts } = {}, name) => {
    beforeRun?.(name);
    const temp = tempFor(name);
    const r = spawnSync(cmd, args, { stdio, ...prepare(opts, keepGit, temp) });
    const output = stdio === "pipe" ? `${r.stdout ?? ""}${r.stderr ?? ""}` : undefined;
    // A command that cannot even start is a failure, as before the engine was extracted.
    if (r.error) return { status: "fail", detail: r.error.message, output, ...settle(temp, false) };
    return { status: r.status === 0 ? "ok" : "fail", output, ...settle(temp, r.status === 0) };
  };
  // Same verdict as `launch`, without blocking: output always captured (`launched`: the command did start).
  const launchAsync = (cmd, args, { keepGit = false, ...opts } = {}, name) =>
    new Promise((resolve) => {
      let output = "";
      let done = false;
      const temp = tempFor(name);
      const end = (r) => {
        if (!done) resolve({ ...r, output, launched: true, ...settle(temp, r.status === "ok") });
        done = true;
      };
      const child = spawn(cmd, args, { stdio: ["ignore", "pipe", "pipe"], ...prepare(opts, keepGit, temp) });
      for (const stream of [child.stdout, child.stderr]) stream.on("data", (d) => (output += d));
      child.on("error", (e) => end({ status: "fail", detail: e.message }));
      child.on("close", (code) => end({ status: code === 0 ? "ok" : "fail" }));
    });
  // The project is compiled once per run, by the first check that needs it (`needsBuild`); a failed build fails its dependents.
  let built;
  let buildReported = false;
  const buildVerdict = (name, r) => {
    if (r.status === "ok") return null;
    const output = buildReported ? undefined : r.output;
    buildReported = true;
    return { status: "fail", message: T.buildFailed(name), output, launched: output !== undefined };
  };
  const buildOpts = { shell: true };
  const build = (name, o) => {
    if (!cfg.build) return { status: "fail", message: T.buildMissing(name) };
    built ??= launch(cfg.build, [], { ...buildOpts, stdio: o.stdio }, "build");
    return buildVerdict(name, built);
  };
  const buildAsync = async (name) => {
    if (!cfg.build) return { status: "fail", message: T.buildMissing(name) };
    buildPending ??= built ? Promise.resolve(built) : launchAsync(cfg.build, [], buildOpts, "build");
    built = await buildPending;
    return buildVerdict(name, built);
  };
  let buildPending;
  const list = [];
  // `parallel` (setting, off by default): checks that only run a command also get `runAsync`, started together by `startParallel`.
  const parallel = cfg.parallel === true;
  const add = (id, name, run, runAsync) =>
    list.push({ id, name, run: (o = {}) => run(o, name), ...(runAsync && parallel ? { runAsync: () => runAsync(name) } : {}) });
  // A commit staging documentation only (docs/ or Markdown, not isCode()) does not replay the tests on the working tree (QUA-035);
  // push and CI always do.
  const docsOnly = Boolean(staged?.length) && staged.every((f) => /^docs\//.test(normalize(f)) || /\.md$/i.test(f));

  add("secrets", T.secrets, (o, name) => {
    if (!full) return { status: "not-applicable", detail: T.minimalMode };
    const gitleaks = findGitleaks();
    if (!gitleaks) return { status: "not-run", detail: T.gitleaksMissing };
    if (spawnSync("git", ["rev-parse", "--git-dir"], { cwd: root, stdio: "ignore" }).status !== 0)
      return { status: "not-run", detail: T.noGit, configured: true };
    // In CI (or for `drwil verify`) nothing is staged to judge: the whole history is analysed, not an empty index.
    const args = whole ? ["git", "--no-banner", "--redact", "."] : ["git", "--pre-commit", "--staged", "--no-banner", "--redact", "."];
    // Must see the index of the commit in progress: under `git commit -a`, the one named by GIT_INDEX_FILE.
    return launch(gitleaks, args, { ...o, keepGit: true }, name);
  });

  add("docs-references", T.docs, (o, name) => launch(process.execPath, [".githooks/check-docs.mjs"], o, name));
  add("contract-tests", T.contractTests, (o, name) => launch(process.execPath, [".githooks/check-contract-tests.mjs"], o, name));
  add("architecture-map", T.architecture, (o, name) => launch(process.execPath, [".githooks/check-architecture.mjs"], o, name));

  // Brief scope gate: at commit time the local state; elsewhere (push, CI, verify) each commit of the branch, through its trailer.
  add("scope-gate", T.scope, (o, name) => {
    o.beforeRun?.(name);
    const r = checkScope(root, { cfg, staged, env });
    const output = r.problems.map((p) => `  ${r.status === "fail" ? "✗" : "⚠"} ${p}`).join("\n");
    if (output && o.stdio !== "pipe") console.log(output);
    return { status: r.status, message: r.message, detail: r.detail, configured: r.configured, output };
  });

  // QUA-017: never work directly on the main branch, including the very first commit (`init()` always creates a working branch first).
  // `symbolic-ref` works even before the first commit. Never in CI (a merge landing on master is legitimate), nor on a push of tags
  // only (DRWIL_PUSH_TAGS_ONLY) or of branch deletions (DRWIL_PUSH_NO_BRANCH), both set by .githooks/pre-push. At push time, the pushed
  // remote branches (DRWIL_PUSH_REFS) are judged, not the current one: `recette/vX.Y.Z` pushed from master does not touch master.
  add("main-branch", T.branch, () => {
    if (env.CI) return { status: "not-applicable", detail: T.branchCi };
    if (env.DRWIL_PUSH_TAGS_ONLY === "1") return { status: "not-applicable", detail: T.branchTags };
    if (env.DRWIL_PUSH_NO_BRANCH === "1") return { status: "not-applicable", detail: T.branchNoBranch };
    if (env.DRWIL_PUSH_REFS?.trim()) {
      const pushed = env.DRWIL_PUSH_REFS.trim()
        .split(/\s+/)
        .map((r) => r.replace(/^refs\/heads\//, ""));
      const main = pushed.find((b) => ["master", "main"].includes(b));
      return main ? { status: "fail", message: T.branchProtected(main) } : { status: "ok" };
    }
    const branch = spawnSync("git", ["symbolic-ref", "--short", "HEAD"], { cwd: root, encoding: "utf8" });
    const branchName = branch.status === 0 ? branch.stdout.trim() : null;
    if (branchName && ["master", "main"].includes(branchName)) return { status: "fail", message: T.branchProtected(branchName) };
    return { status: "ok" };
  });

  const selfTests = () => {
    if (!full) return { status: "not-applicable", detail: T.minimalMode };
    const dir = join(root, ".githooks");
    const tests = existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith(".test.mjs")) : [];
    // These tests try drwil's code: an equipped project does not receive them.
    if (!tests.length) return { status: "not-applicable", detail: T.noTests };
    if (docsOnly) return { status: "not-run", detail: T.docsOnly };
    return { args: ["--test", ...tests.map((f) => join(".githooks", f))] };
  };
  add(
    "checks-self-test",
    T.selfTests,
    (o, name) => {
      const t = selfTests();
      return t.args ? launch(process.execPath, t.args, o, name) : t;
    },
    async (name) => {
      const t = selfTests();
      return t.args ? launchAsync(process.execPath, t.args, {}, name) : t;
    },
  );

  add("ci-coverage", T.ciCoverage, (o, name) => {
    if (!full) return { status: "not-applicable", detail: T.minimalMode };
    const ciFiles = Array.isArray(cfg.ciFiles) ? cfg.ciFiles : [];
    if (!ciFiles.length) return { status: "not-run", detail: T.noCi, configured: true };
    return launch(process.execPath, [".githooks/check-control-coverage.mjs"], o, name);
  });

  // Stack checks (lint, typecheck, tests) are declared by the project. At pre-commit only (`staged` given), a check declaring `paths`
  // is deferred if no staged file matches: it stays "not run" (QUA-013), and the pre-push as well as CI replay it.
  for (const c of Array.isArray(cfg.checks) ? cfg.checks : []) {
    const name = c.name ?? c.run;
    const skipped = () => {
      if (docsOnly) return { status: "not-run", detail: T.docsOnly };
      if (staged && Array.isArray(c.paths) && c.paths.length) {
        const patterns = c.paths.map(globToRegex);
        if (!staged.some((f) => patterns.some((p) => p.test(f)))) return { status: "not-run", detail: T.deferred };
      }
      return null;
    };
    const cwd = c.cwd ? join(root, c.cwd) : root;
    list.push({
      id: c.id ?? `project:${name}`,
      name,
      // QUA-141: what the check declares, for its own fingerprint (a project check only).
      declaration: JSON.stringify([c, c.needsBuild ? cfg.build : null]),
      paths: Array.isArray(c.paths) && c.paths.length ? c.paths : null,
      run: (o = {}) => skipped() ?? (c.needsBuild ? build(name, o) : null) ?? launch(c.run, [], { ...o, shell: true, cwd }, name),
      ...(parallel && c.parallel !== false
        ? {
            runAsync: async () =>
              skipped() ?? (c.needsBuild ? await buildAsync(name) : null) ?? launchAsync(c.run, [], { shell: true, cwd }, name),
          }
        : {}),
    });
  }
  // QUA-136: a check that ran reports `durationMs` (whole ms, its own on a parallel pass); not-run and not-applicable have none, never 0.
  const timed = (fn) => (o) => {
    const start = performance.now();
    const end = (r) => (r.status.startsWith("not-") ? r : { ...r, durationMs: Math.round(performance.now() - start) });
    const r = fn(o);
    return r instanceof Promise ? r.then(end) : end(r);
  };
  for (const c of list) for (const k of ["run", "runAsync"]) if (c[k]) c[k] = timed(c[k]);
  // QUA-140: `reuse: false` in the project's check declares it always replayed (it reads the network, the forge, the clock).
  const declared = Array.isArray(cfg.checks) ? cfg.checks : [];
  // QUA-141: a check is also reused on its own (`perControl`) when it declares `paths` and neither `reuse: false` nor `reuse: "tree"`.
  for (const c of list) {
    const reuse = declared.find((d) => (d.id ?? `project:${d.name ?? d.run}`) === c.id)?.reuse;
    c.reuse = reuse !== false;
    c.perControl = c.reuse && reuse !== "tree" && Array.isArray(c.paths);
  }
  list.fingerprints = () => controlFingerprints(root, list);
  list.finish = finish;
  return list;
}

/**
 * Starts the `runAsync` of the checks that have one, in list order, at most `limit` at a time (by default half the cores, two at
 * least: the suites a check launches are parallel themselves): returns a Map id → Promise of the result. The caller reads them in
 * the list order, so the report is the one of a sequential run.
 */
export function startParallel(list, limit = Math.max(2, availableParallelism() >> 1)) {
  const waiting = new Map();
  const pending = list.filter((c) => c.runAsync);
  for (const c of pending) waiting.set(c.id, Promise.withResolvers());
  let active = 0;
  const next = () => {
    while (active < Math.max(1, limit) && pending.length) {
      const c = pending.shift();
      const { resolve, reject } = waiting.get(c.id);
      active++;
      c.runAsync()
        .then(resolve, reject)
        .finally(() => {
          active--;
          next();
        });
    }
  };
  next();
  return new Map([...waiting].map(([id, d]) => [id, d.promise]));
}
