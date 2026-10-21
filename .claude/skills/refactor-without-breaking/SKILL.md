---
name: refactor-without-breaking
description: Split or restructure existing code without changing its behavior — characterization tests first, small committed steps, bugs found committed separately. Use for every refactor.
---

Recipe: `docs/recipes/refactor-without-breaking.md` (characterization tests
first, small steps, pitfalls already encountered). Context: the touched
layer's `AGENTS.md`. A bug found along the way: separate commit
(`AGENTS.md`, Conduct section).
Verify: `node .githooks/run-checks.mjs` at every step.
