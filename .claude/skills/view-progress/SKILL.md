---
name: drwil-progress
description: Generate a static HTML dashboard (project progress, contract coverage) with nothing to deploy. Use for a quick overview before a meeting, resuming a project, or a review.
---

Recipe: `docs/recipes/view-progress.md` (copy the optional module from
`templates/common/optional/dashboard/` to
`.githooks/dashboard.mjs` (to create), run it by hand with
`node .githooks/dashboard.mjs`, never in CI or at commit time).
Nothing is recomputed: the dashboard copies the "Status" line and increments as
written in each `docs/projects/` brief.
