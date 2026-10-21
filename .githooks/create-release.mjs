#!/usr/bin/env node
// Puts a Git tag (vX.Y.Z) + a GitHub release note on the current commit, to trace the merged
// workstreams without publishing to npm.
// NEVER modifies a tracked file (QUA-017: no commit on the main branch): only a Git tag and a
// GitHub release (outside the repository) are created. Version: that of the repository's
// publishable npm packages (package.json, single source decided and reviewed in a PR); without an
// npm package, computed from the commit messages the Conventional Commits way, best-effort (safe
// default: "patch" bump if no type is recognised).
// `--candidate` (QUA-051): publishes the version as a candidate pre-release (vX.Y.Z-rc.N) with its
// tarball and its generated acceptance sheet (acceptance-sheet.mjs); the final release only comes
// from promote-release.mjs, after the human verdict (release-verdict.mjs).
// Usage: node create-release.mjs [--candidate] [--dry-run]
import { execFileSync } from "node:child_process";
import { pathToFileURL } from "node:url";
import { readFileSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";

const dryRun = process.argv.includes("--dry-run");
const candidate = process.argv.includes("--candidate");

const TEXTS = {
  fr: {
    disagree: "paquets en désaccord sur la version ({0}) : les aligner dans une PR",
    notSemver: "version de paquet absente ou non semver ({0}) dans {1}/package.json",
    fix: "changer la version dans une PR (npm version patch|minor|major --no-git-tag-version)",
    exists: "{0} existe déjà : {1}",
    notHigher: "{0} n'est pas supérieure au dernier tag {1} : {2}",
    packFailed: '[create-release] "npm pack" a échoué pour {0}, tarball non attaché : {1}',
    nothing: "[create-release] rien à publier depuis {0} (aucun commit).",
    lastTag: "[create-release] dernier tag : {0}",
    none: "(aucun)",
    refused: "[create-release] ✗ release refusée : {0}",
    suggestion: "suggestion d'après les commits",
    detected: "bump détecté",
    bump: "[create-release] {0} : {1} ({2} commit(s) analysé(s))",
    next: "[create-release] prochaine version : {0} (source : {1})",
    tarballs: "[create-release] tarball(s) à construire et attacher à la release : {0}",
    dryRun: "[create-release] --dry-run : aucun tag ni release créé.",
    pushed: "[create-release] tag {0} créé et poussé.",
    attached: "[create-release] tarball(s) attaché(s) à la release : {0}",
    ghFailed: "[create-release] tag créé, mais la release GitHub a échoué (gh indisponible ou permission insuffisante) : {0}",
    catchUp: "[create-release] rattraper à la main : {0}",
    candidate: "[create-release] candidate : {0} (pré-release ; la release définitive vient de la promotion, après le verdict de recette)",
    sheet: "[create-release] fiche de recette de {0} :",
    prerelease: "[create-release] pré-release {0} publiée avec sa fiche de recette ({1}).",
  },
  en: {
    disagree: "packages disagree on the version ({0}): align them in a PR",
    notSemver: "package version missing or not semver ({0}) in {1}/package.json",
    fix: "change the version in a PR (npm version patch|minor|major --no-git-tag-version)",
    exists: "{0} already exists: {1}",
    notHigher: "{0} is not higher than the latest tag {1}: {2}",
    packFailed: '[create-release] "npm pack" failed for {0}, tarball not attached: {1}',
    nothing: "[create-release] nothing to publish since {0} (no commit).",
    lastTag: "[create-release] latest tag: {0}",
    none: "(none)",
    refused: "[create-release] ✗ release refused: {0}",
    suggestion: "suggestion from the commits",
    detected: "detected bump",
    bump: "[create-release] {0}: {1} ({2} commit(s) analysed)",
    next: "[create-release] next version: {0} (source: {1})",
    tarballs: "[create-release] tarball(s) to build and attach to the release: {0}",
    dryRun: "[create-release] --dry-run: no tag nor release created.",
    pushed: "[create-release] tag {0} created and pushed.",
    attached: "[create-release] tarball(s) attached to the release: {0}",
    ghFailed: "[create-release] tag created, but the GitHub release failed (gh unavailable or insufficient permission): {0}",
    catchUp: "[create-release] catch up by hand: {0}",
    candidate: "[create-release] candidate: {0} (pre-release; the final release comes from the promotion, after the acceptance verdict)",
    sheet: "[create-release] acceptance sheet of {0}:",
    prerelease: "[create-release] pre-release {0} published with its acceptance sheet ({1}).",
  },
};

/** Fills the `{0}`, `{1}`… holes of a message. */
const fill = (message, ...values) => message.replace(/\{(\d)\}/g, (_, i) => values[i]);

/** Messages in the project's language (.drwil/ia-first.json of the current folder; French by default). */
export function texts() {
  try {
    return TEXTS[JSON.parse(readFileSync(join(".drwil", "ia-first.json"), "utf8")).lang === "en" ? "en" : "fr"];
  } catch {
    return TEXTS.fr;
  }
}

function git(args) {
  return execFileSync("git", args, { encoding: "utf8" }).trim();
}

function latestTag() {
  try {
    // stderr captured (not "inherit"): no tag is the normal state of a first call, not an error
    // to show to the user.
    // A candidate (vX.Y.Z-rc.N) is never the latest final tag: the range and the version start
    // from the last final release.
    return execFileSync("git", ["describe", "--tags", "--abbrev=0", "--match", "v[0-9]*.[0-9]*.[0-9]*", "--exclude", "*-*"], {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "ignore"],
    }).trim();
  } catch {
    return null; // no tag: first release
  }
}

function commitsSince(tag) {
  const range = tag ? `${tag}..HEAD` : "HEAD";
  const out = git(["log", range, "--no-merges", "--pretty=%s%x1f%b%x1e"]);
  if (!out) return [];
  return out
    .split("\x1e")
    .map((s) => s.trim())
    .filter(Boolean)
    .map((s) => {
      const [subject, body = ""] = s.split("\x1f");
      return { subject: subject.trim(), body: body.trim() };
    });
}

// Detects the semver bump from a list of commits (Conventional Commits, best-effort).
// Exported for the unit test.
export function detectBump(commits) {
  if (commits.length === 0) return null;
  let bump = "patch"; // safe default: every merged workstream deserves at least a patch
  for (const { subject, body } of commits) {
    const m = /^(\w+)(\([^)]*\))?(!)?:\s/.exec(subject);
    const breaking = (m && m[3] === "!") || /BREAKING CHANGE/.test(body) || /BREAKING CHANGE/.test(subject);
    if (breaking) return "major"; // announced break: absolute priority, no need to go on
    if (m && m[1] === "feat") bump = "minor";
  }
  return bump;
}

export function nextVersion(tag, bump) {
  const base = tag ? tag.replace(/^v/, "") : "0.0.0";
  let [maj, min, pat] = base.split(".").map((n) => parseInt(n, 10) || 0);
  if (bump === "major") {
    maj += 1;
    min = 0;
    pat = 0;
  } else if (bump === "minor") {
    min += 1;
    pat = 0;
  } else {
    pat += 1;
  }
  return `v${maj}.${min}.${pat}`;
}

// Finds the repository's publishable npm packages (package.json tracked by Git, with a "name" and
// without "private": true) — ignores the root package.json of a private monorepo (workspaces) and
// any package explicitly marked private. Best-effort: an unreadable/invalid package.json is
// ignored, not an error.
export function publicPackages() {
  let files;
  try {
    files = git(["ls-files", "--", "package.json", "*/package.json", "*/*/package.json", "*/*/*/package.json"]).split("\n").filter(Boolean);
  } catch {
    return [];
  }
  const dirs = [];
  for (const f of files) {
    try {
      const pkg = JSON.parse(readFileSync(f, "utf8"));
      if (pkg.private || !pkg.name) continue;
      dirs.push(dirname(f));
    } catch {
      // unreadable or invalid package.json: ignored, not blocking for the release
    }
  }
  return dirs;
}

// Versions declared by the publishable packages: [{ dir, version }].
export function packageVersions(dirs) {
  return dirs.map((dir) => {
    try {
      return { dir, version: JSON.parse(readFileSync(join(dir, "package.json"), "utf8")).version ?? null };
    } catch {
      return { dir, version: null };
    }
  });
}

function compare(a, b) {
  const p = (v) =>
    v
      .replace(/^v/, "")
      .split(".")
      .map((n) => parseInt(n, 10) || 0);
  const [x, y] = [p(a), p(b)];
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] - y[i];
  return 0;
}

// Decides the release version (pure, exported for the tests). With npm packages, the tag follows
// their package.json: a single number for the tag, the tarball and `--version`, never two that
// diverge. Returns { next, source } or { error }; null if there is nothing to publish.
export function decideVersion({ latest, bump, versions = [], existingTags = [] }, T = texts()) {
  if (!bump) return null;
  if (!versions.length) return { next: latest ? nextVersion(latest, bump) : "v0.1.0", source: "commits" };
  const distinct = [...new Set(versions.map((v) => v.version))];
  if (distinct.length > 1) return { error: fill(T.disagree, versions.map((v) => `${v.dir}@${v.version}`).join(", ")) };
  if (!/^\d+\.\d+\.\d+$/.test(distinct[0] ?? "")) return { error: fill(T.notSemver, distinct[0], versions[0].dir) };
  const next = `v${distinct[0]}`;
  if (existingTags.includes(next)) return { error: fill(T.exists, next, T.fix) };
  if (latest && compare(next, latest) <= 0) return { error: fill(T.notHigher, next, latest, T.fix) };
  return { next, source: `package.json (${versions.map((v) => v.dir).join(", ")})` };
}

/** Next candidate tag of a version: `vX.Y.Z-rc.N`, N one more than the highest existing (QUA-051). */
export function candidateTag(version, existingTags) {
  const prefix = `${version}-rc.`;
  const n = existingTags.filter((t) => t.startsWith(prefix)).map((t) => parseInt(t.slice(prefix.length), 10) || 0);
  return `${prefix}${Math.max(0, ...n) + 1}`;
}

// Builds a tarball (npm pack) for each publishable package found, in a temporary folder, and
// returns the paths of the produced .tgz.
function buildTarballs(dirs, T) {
  const out = [];
  if (dirs.length === 0) return out;
  const dest = mkdtempSync(join(tmpdir(), "create-release-"));
  for (const dir of dirs) {
    try {
      const packed = execFileSync("npm", ["pack", "--silent", "--pack-destination", dest], { cwd: dir, encoding: "utf8" }).trim();
      out.push(join(dest, packed.split("\n").pop()));
    } catch (e) {
      console.error(fill(T.packFailed, dir, e.message));
    }
  }
  return out;
}

// Candidate pre-release: tarballs and sheet first (a failure leaves no tag), then the tag, then the
// pre-release carrying the tarballs and the sheet (attached, and as its note: GitHub shows it).
async function publishCandidate({ tag, next, packages, existingTags }, T) {
  const { buildSheet } = await import("./acceptance-sheet.mjs");
  const rc = candidateTag(next, existingTags);
  console.log(fill(T.candidate, rc));
  const tarballs = dryRun ? null : buildTarballs(packages, T);
  const sheet = buildSheet(process.cwd(), { tag: rc, version: next, from: tag, tarballs, packages: packages.length ? packages : null });
  if (dryRun) {
    console.log(`${fill(T.sheet, rc)}\n\n${sheet}`);
    console.log(T.dryRun);
    return;
  }
  const file = join(mkdtempSync(join(tmpdir(), "create-release-")), `recette-${rc}.md`);
  writeFileSync(file, sheet);
  git(["tag", "-a", rc, "-m", `Candidate ${rc}`]);
  git(["push", "origin", rc]);
  console.log(fill(T.pushed, rc));
  const args = ["release", "create", rc, ...tarballs, file, "--prerelease", "--title", rc, "--notes-file", file];
  try {
    execFileSync("gh", args, { stdio: "inherit" });
    console.log(fill(T.prerelease, rc, file));
  } catch (e) {
    console.error(fill(T.ghFailed, e.message));
    console.error(fill(T.catchUp, `gh ${args.join(" ")}`));
  }
}

async function main() {
  const T = texts();
  const tag = latestTag();
  const commits = commitsSince(tag);
  const bump = detectBump(commits);
  const packages = publicPackages();
  const existingTags = git(["tag", "-l", "v*"]).split("\n").filter(Boolean);
  const decision = decideVersion({ latest: tag, bump, versions: packageVersions(packages), existingTags }, T);
  if (!decision) {
    // Without a commit to publish, there is necessarily a tag: a repository without commits fails at `git log`.
    console.log(fill(T.nothing, tag));
    return;
  }
  console.log(fill(T.lastTag, tag ?? T.none));
  if (decision.error) {
    console.error(fill(T.refused, decision.error));
    process.exitCode = 1;
    return;
  }
  const next = decision.next;
  console.log(fill(T.bump, packages.length ? T.suggestion : T.detected, bump, commits.length));
  console.log(fill(T.next, next, decision.source));
  if (packages.length > 0) console.log(fill(T.tarballs, packages.join(", ")));
  if (candidate) return publishCandidate({ tag, next, packages, existingTags }, T);
  if (dryRun) {
    console.log(T.dryRun);
    return;
  }
  const notes = commits.map((c) => `- ${c.subject}`).join("\n");
  git(["tag", "-a", next, "-m", `Release ${next}\n\n${notes}`]);
  git(["push", "origin", next]);
  console.log(fill(T.pushed, next));
  const tarballs = buildTarballs(packages, T);
  try {
    execFileSync("gh", ["release", "create", next, ...tarballs, "--generate-notes"], { stdio: "inherit" });
    if (tarballs.length > 0) console.log(fill(T.attached, tarballs.join(", ")));
  } catch (e) {
    console.error(fill(T.ghFailed, e.message));
    console.error(fill(T.catchUp, `gh release create ${next} --generate-notes${tarballs.length ? ` ${tarballs.join(" ")}` : ""}`));
  }
}

// Only runs when launched directly (node create-release.mjs), not when detectBump/nextVersion are
// imported for a unit test. pathToFileURL (not a "file://" + argv[1] concatenation): on Windows
// process.argv[1] uses backslashes (D:\...) while import.meta.url uses slashes (file:///D:/...) —
// the naive comparison silently fails and main() never runs.
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) await main();
