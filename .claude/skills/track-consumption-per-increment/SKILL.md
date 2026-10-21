---
name: drwil-usage
description: Measure the tokens consumed per workstream and record the closing workstream's line in .drwil/usage.jsonl. Use when closing a workstream, or when asked what a workstream cost, to feed the dashboard and the analysis reports.
---

Recipe: `docs/recipes/track-consumption-per-increment.md`. Measure:
`node .githooks/consumption.mjs` (per workstream, read from Claude Code's
session logs; `--json`, `--since YYYY-MM-DD`). At closing:
`node .githooks/consumption.mjs --record <brief>` appends the line to
`.drwil/usage.jsonl` (if present), never twice. Another AI tool: estimate
by hand (Copilot CLI: `session_store_sql`). Read by the dashboard and the
analysis skills ("Consumption overview"). Never for billing or blocking a
commit — purely informative.
