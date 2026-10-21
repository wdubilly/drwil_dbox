---
name: drwil-value
description: Analyze the repository (scoping, existing code) to identify the business-value features most worth building next, and produce docs/value-discovery.md. Use to frame the next product priority.
---

Recipe: `docs/recipes/discover-product-value.md`. Reads inputs via
`.drwil/ia-first.json` (briefs under `dirs.intentions`, `dirs.projects`,
code under the declared `layers`) rather than hardcoded paths. Generates
or updates docs/value-discovery.md (to create — OPT-N table + detailed
analysis + next step). Never in CI or at commit time: an on-demand
analysis, not a check. "Approve OPT-X" leads to an intention brief to be
cadré, never to direct code (QUA-016).
