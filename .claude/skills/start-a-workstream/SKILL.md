---
name: drwil-start
description: Start a workstream, or a queue of workstreams, with one human gesture — open a poll of the scoped briefs; the human's answer, read by the tool, moves governance to BUILD (when several briefs are chosen, the next ones are queued and started by the agent after each closing). Use when the human wants to start implementing one or several briefs.
---

# Start a workstream (`/drwil-start`)

Starting is the human's decision: it is **their answer to the poll**, read by a tool hook
(`.claude/hooks/prompt-hook.mjs`), that makes the transition — never the agent.

1. List the scoped briefs: `node .githooks/state.mjs briefs --json` (`scope` block committed,
   brief neither finished nor a reference). None: say so and stop. The list is **already
   ordered**: ready briefs first (`ready`: no open `[decision]`), then by `maturity` (`ready`,
   then `in-progress` and `unset`, then `refine`), then by index `priority`;
   `decisions` counts those still to decide.
2. Read the "Proposed order" sections of the intentions (`docs/intentions/`): they may put one
   brief before another of the same rank. Also note the index's P1 intentions that have no framed
   brief yet.
3. Open **one** poll (Claude Code: `AskUserQuestion`; Copilot CLI: `ask_user`), in that order,
   preceded by a short message citing the P1 intentions "to frame" (never offered for start):
   - Claude Code: `header` exactly `drwil-start`; one option per brief, **label = exact path**
     (e.g. `docs/projects/<brief>.md`), description = title. At most 4 options, the first of the
     list; a brief that is not ready keeps its place at the end, its description says "not
     ready: N decision(s) to make first"; a brief with `maturity` `refine` is never hidden, its
     description starts with "to refine: review before starting it"; the human can type another
     path under "Other". `multiSelect: true` when the human wants to chain several briefs (a
     queue): the briefs ticked are run in the order of the poll's options, whatever the order
     of the clicks (the hook sorts them), the first now, the next ones
     started by the agent after each closing; only ready briefs (maturity « ready », no open
     decision, acceptance criteria) can be queued, otherwise the whole start is refused.
   - **A single scoped brief**: a question needs at least two options, so add a second one,
     label `Not now` (description: start nothing), never recommended; the hook reads it,
     starts nothing and says so.
   - Claude Code, **second question** of the same poll: `header` exactly `drwil-merge`,
     options "Automatic merge" (the agent will schedule the merge in CLOSING) and "Manual
     merge", with no recommendation: it is the acceptance decision, taken at start.
   - Copilot CLI: `message` starting with `drwil-start :`; a first field whose `enum` = the exact
     paths, and a **second field named exactly `drwil-merge`** with `enum` "Automatic merge" and
     "Manual merge" (no `default`, no recommendation): the hook reads `fiche=…, drwil-merge=…`.
     No `drwil-merge` answer read: manual merge.
   - **Recommend**: the first ready brief, first, with "(Recommended)" and the reason in
     its **description** — never in the label.
   - **Forbidden**: filling `answers` or a default value (`default`): the hook refuses the poll.
     The human chooses alone.
4. After the answer, read the state again: `node .githooks/state.mjs`. In BUILD on the
   chosen brief: implement. Otherwise: report the refusal as is, without retrying another way;
   the hook always says why (`[drwil]` message: refusal, « Not now », no answer read, error).
   No `[drwil]` message at all: the hooks are not active in this session (e.g. opened before
   `init`) — say so and suggest restarting the session. If the state
   says "Automatic merge authorised": in CLOSING, after verify PASS and pushing the closing
   commit, in this order — wait until GitHub has computed the PR and, if it is behind the main
   branch (BEHIND), update it; schedule `gh pr merge --auto --merge`; check that the merge is
   scheduled (`gh pr view --json autoMergeRequest`); only then go back to Scoping, which clears
   the authorisation. On failure, stay in CLOSING and report it. If `gh pr merge --auto` fails
   because the repository does not allow it (« Auto merge is not allowed for this repository »),
   say « auto-merge not allowed by the repository » and the way out — a repository admin ticks
   « Allow auto-merge » in the GitHub repository settings (General → Pull Requests), or the
   human merges by hand — stay in CLOSING, and never say the merge is scheduled: a mergeable PR,
   auto-merge allowed by the repository, auto-merge scheduled and merge done are four different
   states. Otherwise the merge stays a human gesture: never schedule `gh pr merge --auto` without
   the "Automatic merge" answer, in any tool. Once the PR is merged and the main branch updated,
   the next `node .githooks/state.mjs start` or `next` goes back to SCOPING by itself (brief deleted,
   closing PR merged): no `move SCOPING` to run.
5. **Subagent from the first workstream** (Claude Code and Copilot CLI, queue or not): run each
   workstream in a subagent (fresh context), the first one right after the poll — Claude Code:
   the Agent tool; Copilot CLI: the `task` tool, `general-purpose` agent, `mode: "sync"`. The
   context read again is most of a queue's cost, and grows with each workstream kept in the same
   session. The main session keeps only the reports and the human decisions. A subagent that needs
   the human stops with a report saying why; the main session asks the human, then resumes the
   workstream (a new subagent, whose instructions start by reading the state).
   - The subagent works in the repository folder — never `isolation: "worktree"`:
     `.drwil/state.json` (if present) is ignored by Git, absent from an isolated copy, and the subagent would
     believe it is in SCOPING. It receives no state reminder at start: its instructions start by
     reading the state: `node .githooks/state.mjs`, then carry the workstream until the closing is
     pushed (merge scheduled if authorised; the state then stays CLOSING until the merge), without
     running `next`, and end with the report. It cannot ask the human: on anything that needs one, it stops and says why.
   - One subagent at a time; the main session does not change the state while it works and reads
     it again when it returns. The main session keeps only the reports.
   - **Queue** (the state says « Queue decided at start »): after the closing, the main session follows the PR with `gh` until it is merged, 30 minutes
     at most (`gh pr view <pr> --json state`), updates the main branch, then starts the next brief
     with `node .githooks/state.mjs next` and hands it to a new subagent — no new poll. No next
     brief: the summary.
   - Stop at the first workstream that gets stuck (human attestation awaited, FAIL not fixed, red
     CI, merge not done within 30 minutes, decision to make): say why and what is expected; never
     start the next one on a merge not done. The queue goes on once it is unblocked, or
     `node .githooks/state.mjs queue stop` drops it. A short report at each closing (criteria,
     checks, limits, intents created), a summary at the end of the queue.
   - **Resuming a queue in a new session** (after `/clear` or a restart, the state in SCOPING with
     a queue): first ask « the queue resumes with <brief>: shall I start it? » and run `next` only
     on an explicit yes; anything else (« continue », silence, another request) starts nothing.
   - Another tool (Gemini CLI…): no subagent yet, the queue runs in the same session, the rest
     unchanged.

Without a poll in the tool: give the human the command `node .githooks/state.mjs start`, to run
in an interactive terminal.
