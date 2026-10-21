# Recipe: discover product value

Goal: identify the business-value features most worth building next,
building on what already exists (code, scoping) rather than starting from
a blank page.

Like the risk audit (`docs/recipes/audit-risks-and-debt.md`), this
requires product judgment: no script, a prompt that frames the method for
the AI.

## Inputs to read (generic, no hardcoded path)

Locate inputs via `.drwil/ia-first.json` rather than fixed names (an
assumed docs/01-intentions.md or docs/02-scoping.md, or a src/ folder, may
not exist in this project):

1. **Vision and scoping**: briefs under `dirs.intentions`
   (`docs/intentions/` by default, the need/promise of each topic) and
   `dirs.projects` (`docs/projects/` by default, decisions and
   constraints already settled per project).
2. **Source code**: the folders declared in `layers` and
   `layerPrefixes`/`codePrefixes` from the config. Spot data models, API
   routes and components already ready or half-ready (e.g. data already
   stored but not yet displayed = low effort, immediate value).
3. **Tests and infra**: paths flagged as tests in `layerPrefixes`, plus
   infra files (`ciFiles`, `extraCodeGlobs` such as `docker-compose*.yml`)
   to spot already-stabilized building blocks.

## Method

For each opportunity identified, evaluate:
- **Business value**: concrete benefit for the end user or the project
  (time saved, missing key feature, retention).
- **Technical proximity / effort**: does it build on existing code?
  (data already in the DB but not displayed = low effort).
- **Product alignment**: consistent with the vision already cadré in
  `docs/intentions/` briefs?

## Output

Generate or update `docs/value-discovery.md` (to create) at the project
root, following this template:

```markdown
# Value discovery and product opportunities

> Last scan: YYYY-MM-DD
> Project state: (short summary of the current code state)

## Consumption overview

(if .drwil/usage.jsonl exists: cumulative tokens and minutes, models used
since the last scan — see `docs/recipes/track-consumption-per-increment.md`.
Otherwise, omit this section rather than inventing a figure.)

## Priority opportunities (value / effort matrix)

| ID | Proposed feature | Why (business value) | Existing code state | Effort | Recommended action |
|---|---|---|---|---|---|
| OPT-1 | ... | ... | ... | 🟢 Low / 🟡 Medium / 🔴 High | ... |

## Detailed analysis of the best leads

### OPT-1 — title
- **Problem solved**: ...
- **Reusable existing building blocks**: real files cited
- **What's left to do**: ...
- **Impact if implemented**: ...

## Next step

Pick an ID and reply: "Approve OPT-X to turn it into an intention."
```

## Guardrails

- Every OPT-N line must cite real code and/or a real brief — never an
  unsupported claim.
- "Approve OPT-X" leads to an intention brief (under `dirs.intentions`) to
  be cadré, never to code written directly without a brief — consistent
  with QUA-016.
- Run by hand only, never in CI or at commit time.
