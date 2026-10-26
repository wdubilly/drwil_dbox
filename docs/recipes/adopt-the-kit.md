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
   declare the stack's checks (step 3). Then wire the mandatory guarantees
   (step 3 bis): a new project is at 100 % coverage and has its e2e test
   from the start.
1. **Re-read the detected stack** (`.drwil/ia-first.json`): layers, tools,
   CI. Correct it if `init` got it wrong. Then **build the map**
   `docs/architecture.md` from the real code (what the application does,
   stack, every code folder with its role), in the `docLang` language
   (absent: the project language), in place of the skeleton `init` wrote: drwil maintains it from then on (its check reports a code folder
   it does not cite). On a project that already had indexed code, `init`
   also wrote the reference brief « existing code »
   (file existing-code.md of `docs/projects/`): its `scope` block lists every code
   file indexed before the kit, and the map's « Existing code » section
   cites their folders with a role « to describe at adoption » — so that the
   first commit, even of a brief alone, passes. Replace those roles with
   the real ones here. The brief then stays: a file a workstream touches
   enters the `scope` block of that workstream's brief, as any code; a
   deleted file leaves its list; the brief is kept as long as it covers a
   file. `init` never rewrites it, an update never creates it and
   `uninstall` leaves it in place.
2. **Fill in the layer `AGENTS.md` files** (one per `layers` entry):
   Context, Test pitfalls, Layer contracts, Style and recipes, Verify —
   from the existing code, not generalities.
3. **Declare the stack's checks** in `.drwil/ia-first.json` → `checks`
   (lint, typecheck, tests, build…): each with a `name` and a `run`, run by
   `node .githooks/run-checks.mjs` and at commit time.
3 bis. **Wire the mandatory guarantees**: `npx drwil guarantees` (once the
   stack is known; `--dry-run` to preview). For every layer whose stack is
   in its table (Node/TypeScript, Python, Go, JVM), it declares the coverage
   check in `checks`: 100 % on lines, branches, functions and statements —
   as far as the tool measures them, said as is (Go: statements only) — on
   a new project; on an existing project in test debt, the measured level
   as a ratchet (never lowered), 100 % on new or modified code (diff
   coverage) and a non-blocking `[P1]` brief to climb to 100 %. A stack
   outside the table, a tool missing or not launchable, no `e2e` check, an
   incomplete forge (Git, GitHub or GitLab remote, CI replaying the hook's
   checks, protected main branch with required checks and no administrator
   bypass) or a README, « 5 lines » or `docs/features.md` still carrying
   skeleton text: each gets a blocking `[P0]` brief, the only one offered
   by `/drwil-start`, and `drwil verify` is FAIL until it is held. The
   branch protection is set by `npx drwil guarantees --protect` **with the
   requester's agreement** only. No light profile, no waiver: a file
   exclusion (configuration, entry point…) is declared with its reason in
   `.drwil/ia-first.json` → `guarantees.exclusions` (`{ "layer", "path",
   "reason" }`); one without a reason is refused. Each wired check is cited
   by the project's coverage contract (`catalog:QUA-004`) and its e2e
   contract (`catalog:QUA-020`), so that `drwil verify` replays them.
   Limit: 100 % coverage proves every line is run by a test, not that it is
   correct.
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
   - `catalog:QUA-004` (coverage) and `catalog:QUA-020` (e2e): not a
     choice — mandatory guarantees wired at step 3 bis. A new project: 100 %
     and e2e from the start. An existing project in debt: threshold at the
     measured level, diff coverage at 100 %, ratchet; e2e required too.
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
