# Recipe: view progress

Goal: see at a glance how projects (`docs/projects/`) are progressing and
how contracts (`docs/contracts.md`) are covered, without deploying anything
or recomputing a state that lives nowhere else.

Steps:
1. Copy the single file from `templates/common/optional/dashboard/`
   (optional module of the kit package, see its README) to
   `.githooks/dashboard.mjs` (to create).
2. Run it by hand: `node .githooks/dashboard.mjs` — writes
   `docs/dashboard.html` (to create) by default. Never in CI or at
   commit time: it's a viewing tool, not a check.
3. Open the generated file in a browser. Add its path to `.gitignore`
   (a derived artifact, not a source).

What it shows: the pending-work index checkboxes, then for each brief in
`docs/projects/` its raw "Status" line and increments as written; installed
contracts (`docs/contracts.md`) and the not-yet-installed ones from the
catalogue (`docs/contracts-catalog.md`). Nothing is reinterpreted: a brief
without a "Status" line is flagged as such instead of a guessed one.
