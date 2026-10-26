---
name: drwil-adopt
description: Make the kit live on a project after `init` — a new project is scoped first (need, scope, stack and layers, first business rules, risk); an existing one gets its layer AGENTS.md files filled in, its stack's checks declared and the catalog's contracts proposed one by one (each choice stays a decision of the requester). Use when adopting the kit, starting a new project, or resuming an ongoing adoption.
---

Recipe: `docs/recipes/adopt-the-kit.md` (step 0 for a new project, with no
code and no layer yet: scope it — need, first version, stack, layers via
`drwil init --layers`, first business rules, risk — and write `AGENTS.md`,
`docs/architecture.md` and a first `README.md`; then: re-read the stack, fill in
the layer `AGENTS.md` files, declare the checks, propose the contracts
from `docs/contracts-catalog.md` one by one). Once the stack is known, run
`npx drwil guarantees` (step 3 bis): 100 % coverage per layer and an e2e test
of the critical paths on a new project, a ratchet and 100 % diff coverage on
an existing one in debt, a protected forge with a CI and a written
presentation of the product — each guarantee not held gets a blocking `[P0]`
brief, handled first; the branch protection (`--protect`) only with the
requester's agreement. An adopted contract migrates
from the catalog to the registry (`docs/contracts.md`), with its check
actually wired up.
Verify: `node .githooks/run-checks.mjs` at every step, one commit per
subject.
