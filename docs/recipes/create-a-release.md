# Recipe: create a release (tag + GitHub note)

Goal: mark a merged workstream with a Git tag (`vX.Y.Z`) and a release
note, without publishing to npm and without committing any file
(compatible with a "no direct commit on the main branch" contract, if
adopted).

This is **not installed by default**: the script that computes and posts
the release is an optional module, like the dashboard — see the README
in `templates/common/optional/create-release/` (module shipped by the
kit package) to activate it, copied to
`.githooks/create-release.mjs` (to create). Like a commit or a push, this
script only runs on the user's explicit request, never on its own in CI.

## Publication green light

Creating the release (step 3 below) posts a tag and a note that everyone
sees: by default it is a gesture **after agreement** — the agent shows
the `--dry-run` result and waits for the human's yes before creating
anything.

The human can give that yes **at launch** instead, so that a queue of
workstreams run by sub-agents (which cannot ask a question) publishes
without stopping: an explicit sentence in the request that launches the
workstream or the queue, naming the version — e.g. « publication green
light: v1.4.0 ». The agent that orchestrates passes it verbatim to the
sub-agent in charge of the release. With that green light, the agent
runs the `--dry-run`, then creates the release only if the computed
version is the one named and the script refused nothing; otherwise, or
without a green light, it stops after the `--dry-run` and gives the human
the exact command to run. The green light covers one release, never the
following ones, and never an npm publication (see « Assumed limits »).

## Method (once the module is activated)

1. Preview without creating anything: `node .githooks/create-release.mjs
   --dry-run` — shows the last tag, the detected bump (major/minor/patch)
   and the next version.
2. The version: when there are publishable npm packages (tracked
   package.json, with a `name`, without `"private": true`), the tag is
   **`v` + their `version`** — single source, decided in a PR with
   `npm version patch|minor|major --no-git-tag-version`. The script refuses
   (exit 1, `--dry-run` included) a tag that already exists, a version that
   does not increase, or packages that disagree. Without an npm package,
   the version is computed *best-effort* from commit messages since the
   last tag, Conventional-Commits style (`feat:` → minor, announced
   breaking change → major, otherwise patch by default); with packages,
   that computation is only a displayed suggestion. In the same PR, if the
   project keeps a `CHANGELOG.md` (if present), its « Unreleased » section, fed by each
   workstream's closing, is renamed to that version (a new empty
   « Unreleased » above it).
3. Create for real, after the human's agreement or with the green light
   given at launch (see above): `node .githooks/create-release.mjs`
   (annotated tag pushed to `origin`, then `gh release create
   --generate-notes` if `gh` is available and authenticated).
4. If the repo contains one or more publishable npm packages (a
   `package.json`, if present, tracked by Git, with a `name`, without
   `"private": true`), a tarball (`npm pack`) is built for each and
   attached as a downloadable asset on the GitHub release — handy to
   hand the package to someone to try without publishing to npm.
   `--dry-run` lists the packages that would be packed without
   building anything.
5. Wrong version for once (miscalculated tag): delete the tag (`git tag
   -d vX.Y.Z && git push origin :refs/tags/vX.Y.Z`) and rerun once the
   fix is in place — never rewrite shared history.

## Acceptance before publication (candidate, verdict, promotion)

When an acceptance team that does not read the repository validates each
version, its approval conditions the publication: it tests the tarball
before the final release.

1. Candidate: `node .githooks/create-release.mjs --candidate --dry-run`
   shows the version and the generated acceptance sheet without building
   or publishing anything; without `--dry-run` (after the human's yes or
   the publication green light, as above), the tag `vX.Y.Z-rc.N` is pushed
   and a GitHub **pre-release** carries the tarball and the sheet (attached
   and as its note). The sheet is generated, never written by hand: on top,
   the points to validate (human criteria of each brief in its last
   version, known limits — « not tried for real yet » when the limit says
   so —, attestations to redo), each with a stable ID `vX.Y.Z-P<n>` and its
   workstream's PR; below, the non-regression already proven (`drwil
   verify` verdict, not counting the rule against direct work on the main
   branch where the release is created, contracts proven by a test, OS
   tested by the CI, tarball fingerprint).
2. Verdict — **a human gesture, never run by an agent**, like `drwil
   attest`: `node .githooks/release-verdict.mjs vX.Y.Z-rc.N --approve`, or
   `--reserve <point>` / `--refuse <point>` for each point concerned. It is
   recorded on the pre-release with who gave it and when (no commit); each
   reserve and each refusal becomes an issue, which the agent sorts with
   the recipe « fix a defect » (defect to fix or intent).
3. Promotion: `node .githooks/promote-release.mjs vX.Y.Z-rc.N --dry-run`,
   then without `--dry-run` (same green light as a release). Refused
   without a verdict or when refused; otherwise the final release `vX.Y.Z`
   is created on the same commit with the same tarball (fingerprint
   checked), the reserves listed in its note. It becomes `releases/latest`,
   which never shows a pre-release: link a README there once, with no
   commit at each version.

## Assumed limits

- Designed for GitHub (`gh release create`); on GitLab, only the Git tag
  is posted by the script, the release note still needs to be created
  via the GitLab Releases API (best-effort, not covered).
- Never publishes to npm: a separate decision, out of scope for this
  recipe.
