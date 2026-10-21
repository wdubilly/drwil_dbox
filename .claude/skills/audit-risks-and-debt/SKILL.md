---
name: drwil-audit
description: Analyze the repository (actual scoping, code, tests) to detect spec drift, dead code, security blind spots and test gaps, and produce docs/audit-risks.md. Use for a health review before a key milestone or on request.
---

Recipe: `docs/recipes/audit-risks-and-debt.md`. Reads inputs via
`.drwil/ia-first.json` (briefs under `dirs.projects`, `dirs.contracts`,
code under the declared `layers`) rather than hardcoded paths. Generates
or updates docs/audit-risks.md (to create — RSK-N table + detail + next
step). Never in CI or at commit time: an on-demand audit, not a check. A
requested fix ("Fix RSK-X") must first go through a scoping brief if it
touches code not already covered (QUA-016).
