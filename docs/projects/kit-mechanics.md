# Project: kit mechanics (files owned by the kit)

**Status**: reference — mechanics shipped at kit install time, 2026-10-21.

<!-- scope
files:
  - .githooks/run-checks.mjs
  - .githooks/check-docs.mjs
  - .githooks/skill-headers.mjs
  - .githooks/check-contract-tests.mjs
  - .githooks/check-architecture.mjs
  - .githooks/consumption.mjs
  - .githooks/check-control-coverage.mjs
  - .githooks/glob.mjs
  - .githooks/engine.mjs
  - .githooks/contracts.mjs
  - .githooks/glossary.mjs
  - .githooks/risk.mjs
  - .githooks/state.mjs
  - .githooks/state-cli.mjs
  - .githooks/briefs.mjs
  - .githooks/state-texts.mjs
  - .githooks/close.mjs
  - .githooks/detour.mjs
  - .githooks/note.mjs
  - .githooks/scope-gate.mjs
  - .githooks/prepare-commit-msg
  - .githooks/scope.mjs
  - .githooks/pre-commit
  - .githooks/pre-push
  - .githooks/commit-msg
  - .githooks/check-file-size.mjs
  - .githooks/check-file-size.legacy.json
  - .githooks/check-code-rules.mjs
  - .githooks/create-release.mjs
  - .githooks/acceptance-sheet.mjs
  - .githooks/release-verdict.mjs
  - .githooks/promote-release.mjs
  - .githooks/dashboard.mjs
  - .githooks/check-colors.mjs
  - .githooks/check-contrast.mjs

  - .claude/settings.json
  - .github/workflows/ia-first.yml
  - .githooks/doc-coherence.mjs
  - .githooks/fingerprints.mjs
  - .githooks/pr-watch.mjs
  - .githooks/base-checks.mjs
  - .githooks/close-branches.mjs
  - .githooks/worktree.mjs
  - .githooks/closure-proof.mjs
  - .githooks/release-review.mjs
-->

## 1. Need

The commit check (`.githooks/check-docs.mjs`) reports any indexed code file
not covered by a `docs/projects/` brief (scoping reminder, severity set in
`.drwil/ia-first.json` -> `scopeReminder`: `blocking` by default, `warning`
optional). Without this brief, the files the kit itself writes would be
reported from the project's very first commit (blocking if `blocking` is
chosen).

## 2. Out of scope

The project's own application code: each piece of work carries its own
`scope` block in its brief. A small task that does not deserve its own
brief attaches to the project's running-chores brief
(`docs/projects/routine-maintenance.md`).

## Hand-off

- **Last state**: brief shipped at install time, nothing to hand off. If the
  kit adds a new mechanics file (`.githooks/` or an AI tool setting), add it
  here. The kit also ships `.githooks/check-control-coverage.mjs` (QUA-013),
  `.githooks/pre-push` and `.githooks/commit-msg` (shipped disabled) (their
  tests stay in the drwil repository, not shipped), as well as `.githooks/check-file-size.mjs`
  and its legacy-ceiling file `.githooks/check-file-size.legacy.json`, plus
  the empty extension point `.githooks/check-code-rules.mjs` — all three
  shipped but **opt-in** (not run until the project declares them in
  `.drwil/ia-first.json → checks`).
  The optional modules' files (release, dashboard, front quality) are
  covered in advance: enabling them by copying into `.githooks/` never
  blocks a commit, QUA-016 being `blocking` by default.
