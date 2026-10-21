# Recipe: audit risks, technical debt and spec drift

Goal: detect what threatens the project's stability, security or
maintainability — drift between scoping and the actual code, dead code,
security blind spots, test gaps — without guessing: every risk raised must
point to a real file.

Unlike the dashboard (`docs/recipes/view-progress.md`), this requires
judgment (comparing an intent to code): no script, a prompt that frames the
method for the AI.

## Inputs to read (generic, no hardcoded path)

Locate inputs via `.drwil/ia-first.json` rather than fixed names (an
assumed docs/02-scoping.md or src/ folder may not exist in this project):

1. **Scoping and specs**: every brief under `dirs.projects`
   (`docs/projects/` by default), excluding the model
   (`model-*`/`modele-*`). Each brief carries its Need, Decisions and Increments
   — the actual scoping to compare against the code.
2. **Invariants**: `dirs.contracts` (`docs/contracts.md` by default) — the
   rules the code must respect, and `docs/contracts-catalog.md` for what's
   proposed but not yet adopted.
3. **Source code**: the folders declared in `layers` and
   `layerPrefixes`/`codePrefixes` from the config (not an assumed src
   folder). For each layer, its `AGENTS.md` (if present) lists known
   pitfalls and layer-specific contracts to check first.
4. **Tests**: paths flagged as such in `layerPrefixes` (e.g. `tests`,
   `e2e`) or the detected stack's convention.

## Method

Actively look for:
1. **Spec drift**: a brief under `docs/projects/` describes a business
   rule or edge case the code doesn't respect (or handles differently
   without the brief being updated).
2. **Dead / orphan code**: files or dependencies referenced by no scoping
   brief, nor imported anywhere (the scope reminder already warns at commit time
   about code outside any brief — QUA-016 — but doesn't catch dead code
   inside a covered brief).
3. **Security/stability blind spots**: missing input validation, secrets
   in plain text, API/DB calls without error handling.
4. **Test gaps**: a critical or complex routine named in a scoping brief
   with no associated test.

## Output

Generate or update `docs/audit-risks.md` (to create) at the project root,
following this template:

```markdown
# Risk and technical debt audit

> Last scan: YYYY-MM-DD
> Overall health: 🔴 Critical / 🟠 Fragile / 🟢 Stable

## Consumption overview

(if .drwil/usage.jsonl exists: cumulative tokens and minutes, models used
since the last scan — see `docs/recipes/track-consumption-per-increment.md`.
Otherwise, omit this section rather than inventing a figure.)

## Summary

| ID | Risk | Category | Severity | Files involved | Proposed action |
|---|---|---|---|---|---|
| RSK-1 | ... | Spec Drift / Dead code / Security / Tests | 🔴/🟠/🟡 | `real/path` | ... |

## Major risks in detail

### RSK-1 — title
- **Expected** (fiche cited): ...
- **Actually coded** (file cited): ...
- **Risk incurred**: ...
- **Recommended fix**: ...

## Next step

Pick an ID and reply: "Fix RSK-X".
```

## Guardrails

- Every RSK-N line must cite a real brief under docs/projects/ and/or a
  real code file — never an unsupported claim.
- "Fix RSK-X" must not trigger code without a brief: if the fix touches
  code not already covered by a scoping brief, create one first (under
  `dirs.projects` or `dirs.intentions`) before fixing — consistent with
  QUA-016.
- Run by hand only, never in CI or at commit time (like the dashboard).
