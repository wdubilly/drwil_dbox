---
name: drwil-release
description: Post a Git tag and a GitHub release note on the latest merged workstream (optional module, never modifies a tracked file). Use once the module is activated on the project, to preview (--dry-run) then create the release.
---

Recipe: `docs/recipes/create-a-release.md`. Optional module, not
installed by default — first check that `.githooks/create-release.mjs` (if present) exists
(otherwise follow the activation steps described in
the recipe before continuing). Always start with
`node .githooks/create-release.mjs --dry-run` and show the result (last
tag, detected bump, next version) before actually creating anything.
Create the release only after the human's yes, or with the publication
green light given at launch for that version (recipe, « Publication
green light ») — never on a guess, never for an npm publication.
With an acceptance team, publish a candidate instead
(`--candidate`, recipe « Acceptance before publication »); the verdict
(`.githooks/release-verdict.mjs` (if present)) is a human gesture an agent never runs, and the
promotion (`.githooks/promote-release.mjs` (if present), `--dry-run` first) needs the same yes as
a release.
