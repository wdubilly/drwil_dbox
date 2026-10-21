# Recipe: adopt the kit on an existing project

Goal: after `init` (stack and layers detected, mechanics installed,
nothing overwritten), make the kit live on a project that already has
code. Every choice below is a **`[decision]`**: propose, never decide in
the requester's place (`AGENTS.md`, Conduct section).

0. **A new project (no code yet, no layer): scope it first.** With the
   requester, one `[decision]` at a time: the need and its users, the
   scope of the first version, the stack (hence the layers), the first
   business rules (hence the first `APP` contracts, step 5) and the risk
   level. Write the result into `AGENTS.md` (« The project in 5 lines »),
   `docs/architecture.md` (the map, kept current from then on) and a first
   `README.md`; open intents for what is left. Then declare the layers:
   `drwil init --layers <layers>` (nothing to detect yet) creates each
   layer's `AGENTS.md` and the tools' per-layer instructions; fill them in
   with the stack's conventions (structure, build and test commands) and
   declare the stack's checks (step 3).
1. **Re-read the detected stack** (`.drwil/ia-first.json`): layers, tools,
   CI. Correct it if `init` got it wrong. Then **build the map**
   `docs/architecture.md` from the real code (what the application does,
   stack, every code folder with its role), in the `docLang` language
   (absent: the project language), in place of the skeleton `init` wrote: drwil maintains it from then on (its check reports a code folder
   it does not cite).
2. **Fill in the layer `AGENTS.md` files** (one per `layers` entry):
   Context, Test pitfalls, Layer contracts, Style and recipes, Verify —
   from the existing code, not generalities.
3. **Declare the stack's checks** in `.drwil/ia-first.json` → `checks`
   (lint, typecheck, tests, build…): each with a `name` and a `run`, run by
   `node .githooks/run-checks.mjs` and at commit time.
4. **Propose the catalog's contracts** (`docs/contracts-catalog.md`) one by
   one, with their typical check: the requester keeps the `[decision]` to
   adopt each or not. An adopted contract: its catalog line becomes a section (`## ID — title`, Rule, Scope, Source of truth, Proof, Reason, Check and/or Manual) moved from the
   catalog to the project's registry (`docs/contracts.md`), with its check
   actually wired up (not a promise):
   - `catalog:QUA-001` (file size): `.githooks/check-file-size.mjs` is
     already shipped by the kit (opt-in) — only needs declaring in
     `checks` with the root and ceiling chosen by the requester.
   - `catalog:QUA-014` (contrast): copy
     `templates/common/optional/front-quality/` (see its README) if a
     front end is detected, then declare its two checks.
   - `catalog:QUA-004` (coverage) and `catalog:QUA-020` (e2e): adopt in
     that order — a low declared threshold first so as not to block a
     project that is just starting, raised as you go, then automated e2e
     on critical paths as proof of real behavioral coverage.
   - The catalog's other lines have no ready-made script: propose the
     detected stack's usual tool (lint, typecheck, coverage…).
5. **Write the first business contract**: `docs/contracts.md` ships
   `APP-001`, an example to replace. With the requester (`[decision]`), pick
   the project's most critical business rule, write the test that proves
   it (its name starts with the contract's ID: `APP-001: …`), declare it in
   `checks` with an `id`, cite it as `**Check**`, list its test file(s) as
   `**Tests**` (QUA-029 checks they exist and carry the ID) and remove the
   `**Manual**` field: `drwil verify` then replays it at every workstream.
   **One check per business rule**, filtering its tests by the ID
   (`npm test -- -t "APP-001"`, `pytest -k APP_001`, `go test -run APP001`):
   a single check shared by several contracts (one plain `npm test`) cannot
   say which rule a failure breaks — `drwil verify` reports it as not
   attributable. From then on, each workstream links its acceptance criteria
   to such contracts (`docs/projects/model-brief.md`, « Acceptance
   criteria »): created by the workstream, or reused as they are; a
   contract changed on purpose is declared in the brief (« Modified
   contracts »), otherwise closing is refused.
6. **Verify**: `node .githooks/run-checks.mjs` green, then one commit per
   subject (layer filled in, check declared, contract adopted) — never a
   catch-all commit.

Out of scope: rewriting the existing code to bring it up to standard —
that's `docs/recipes/refactor-without-breaking.md`, run next, contract by
contract.
