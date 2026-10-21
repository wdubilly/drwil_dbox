# Recipe: track consumption per increment/project

Goal: know, increment by increment, how many tokens were used, which model was
employed, and how long the setup took — never for billing or blocking a
commit, just to inform project tracking.

## Measure: `node .githooks/consumption.mjs`

With Claude Code, nothing to type: the script reads the project's session
logs (`~/.claude/projects/<project path>/`), where each agent answer
carries its tokens, model, timestamp and git branch. One branch per brief
(`workstream/<brief>`, and its scoping branch) attributes it:

```bash
node .githooks/consumption.mjs                    # per workstream: tokens, weighted, active time, models
node .githooks/consumption.mjs --since 2026-10-01 --json
node .githooks/consumption.mjs --record <brief>   # at closing: one line in .drwil/usage.jsonl
```

- Four token categories (input, output, cache read, cache write) and an
  indicative weighted total (in input-token equivalents: output ×5, cache
  read ×0.1, cache write ×1.25; prices change).
- Active time: the gaps between answers, without pauses over 15 minutes.
- Only the consumption fields are read: never a message content is shown
  or written. Never blocking: without a readable log, it says so.

## Format: `.drwil/usage.jsonl` (if present)

Append-only and versioned, one JSON line per workstream, written at its
closing by `--record` (never twice for the same workstream):

```json
{ "workstream": "add-csv-export", "increment": "all", "tokens": 4200000, "input": 900, "output": 61000, "cache_read": 38000000, "cache_write": 120000, "requests": 64, "model": "claude-opus-5-5", "duration_min": 95, "date": "2026-10-16", "source": "claude-code-logs" }
```

- `workstream`: the brief's file name, without extension.
- `tokens`: the weighted total; the four categories follow.
- `model`: the model, or the models separated by commas.
- `duration_min`: active time; `date`: closing date (`YYYY-MM-DD`).

## Another AI tool

The script only reads Claude Code's logs. Otherwise, append the line by
hand at closing, with `"source": "estimate"`:

- **With Copilot CLI**: query `session_store_sql` (`session_usage` table
  for model and tokens; `turns`/`events` for the start/end timestamps,
  hence `duration_min`).
- **With another AI tool**: estimate (an order of magnitude is enough) and
  fill `model` and `duration_min` as best as possible rather than
  omitting the line.

## Reading: the dashboard

`docs/recipes/view-progress.md` (optional module
`.githooks/dashboard.mjs` (to create)) reads `.drwil/usage.jsonl` (if present)
if it exists
and shows, per project: total tokens, total minutes and the list of
models used. If the file is absent, the dashboard works exactly as
before (no regression).

## The analysis skills

`audit-risks-and-debt` and `discover-product-value` also show, at the top
of their report, a short overview (section "Consumption overview")
summarizing `.drwil/usage.jsonl` (if present) if it exists — never invented if the
file is absent.
