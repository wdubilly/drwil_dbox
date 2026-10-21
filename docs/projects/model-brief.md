# Project: short title

**Status**: scoped on YYYY-MM-DD — increment 1 in progress.
**Risk**: MEDIUM
**Maturity**: to refine | ready | in progress
**Intent**: `docs/intentions/<intent>.md` (when the workstream comes from one)

(maturity: "ready" = fully decided, can be started; "to refine" = points
still open; "in progress" = being built. Optional: a brief without it
is "unset", offered without a maturity warning.)

(status: "scoped on …", "increment 1 in progress"…; a brief that is not a
project (kit mechanics): "reference — …", never offered for launch;
"done on YYYY-MM-DD" only when condensing it: a brief carrying it cannot
stay — `docs/projects/journal.md`, then deletion.)

(risk level, `docs/ia-first.md` section 7: LOW has no brief — attach to
`docs/projects/routine-maintenance.md`; MEDIUM: brief + scope; HIGH: also a
Decisions section and the related contracts cited. A scope touching
`.githooks/`, CI or `.drwil/` requires HIGH; it can only be raised.)

<!-- scope
files:
  - backend/app/routers/example.py
  - frontend/src/components/Example*
-->

(`scope` block: one path or pattern per line, under `files:`; covers
the code files this project touches, for the scoping reminder —
`docs/ia-first.md`, section 7. A pattern that's too broad, `**` or
`scripts/*`, is refused: name the file or use a pattern spanning at least
two folders. List everything the workstream will touch from the start:
the code, **its tests**, and `.drwil/ia-first.json` when a criterion
creates a contract (its own check is declared there) — check-docs warns
otherwise; widening mid-workstream costs a separate commit. A generated
copy is covered by its source when `scopeMirrors` declares it.)

## 1. Need

(what the project must produce, for whom, and why now)

## Acceptance criteria

(mandatory for every workstream brief: without any criterion, starting and
closing are refused — routine maintenance excepted. One line per acceptance
criterion of the request, linked to its proof: the ID of a **contract** in
backticks, never a check alone, e.g. `- An empty export is refused —
`APP-012``, or the `[human]` marker for a human judgment, which points to a
`Manual` contract attested by `drwil attest`. The contract is created by the
workstream, or reused as is (a refactor reuses the existing ones); a created
contract carries the **Origin** field (`<workstream> — « <criterion> »`). The
proof is durable, a contract with a replayable `Check` run at every
workstream: it becomes the business non-regression. One business-level
criterion, not one per technical detail. The tool evaluates, never the
agent.)

- Verifiable criterion — `CONTRACT-ID`
- Human-judgment criterion [human] — `MANUAL-CONTRACT-ID`

## Modified contracts

(only when this project changes an existing contract on purpose: one bullet
per contract, its ID in backticks first, then the former criterion and the
reason, approved by the requester at scoping, never by the agent along the
way. A contract modified in the branch without a line here is a regression:
`drwil verify` reports it and closing is refused.)

- `CONTRACT-ID` — former criterion: « ... » — reason: ... (approved on YYYY-MM-DD)

## 2. Out of scope

(what this project does not cover, to prevent an agent from extending it on
its own)

## 3. Constraints

(technical, business, scheduling)

## 4. Decisions

(decided by the requester, dated)

## 5. Points to decide

- [decision] ...

## 6. Increments

- **Increment 1 — title** [AI|human]: content. Exit criterion: what proves the
  increment is done (green tests, a passing check...).

## 7. Hand-off

- **Last state** (YYYY-MM-DD): ...
- **Uncommitted work**: ...
- **Next step**: [AI|human|decision] ...
