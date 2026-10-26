# Recipe: working with branches and merge/pull requests

Goal: keep the link between a `docs/projects/` brief and the git work that
delivers it when that work goes through a branch and a pull/merge
request, instead of direct commits on the main branch.

## Opening a branch

- Name the branch `workstream/<brief-slug>`, where `<brief-slug>` is the file
  name (without extension) of the relevant `docs/projects/` brief. Example:
  a brief named my-project.md is worked on a branch named
  workstream/my-project.
- The brief already exists (scoping done) before opening the branch — no
  coding without a brief, branch or not.
- `init()` already creates a first working branch
  (`workstream/kit-installation`) right after `git init`, before any commit:
  on a brand-new repository, nobody needs to create a branch by hand for
  the very first commit. Renaming this branch (or creating another one
  named after the current brief) is up to the project.

## During the work

- The brief is updated **on the branch**, just like for direct work on the
  main branch (Status, Increments, Hand-off).
- The brief's Status stays "in progress" as long as the branch isn't
  merged, even if the work on it is finished: "done" is only said once
  the merge has actually happened. An index (`docs/projects/pending.md`)
  that checks a box before the merge would be inconsistent with the real
  state of the main branch.

## Opening the pull/merge request

- The pull/merge request description points to the relevant
  `docs/projects/` brief (the template shipped by the kit, if installed,
  already has this section).
- Checks (`.githooks/run-checks.mjs`) must be
  green before opening the pull/merge request, not only before merging
  it. Their output shows only the failed checks and a summary line (warnings
  counted, full detail in `.drwil/evidence/run-checks.log` (if present)); with Claude
  Code, the `drwil-checks` subagent can run them and return only the verdict
  and the failures, copied word for word.
- Before merging: check that the pull/merge request CI status checks
  are green (`gh pr checks <n> --watch` on GitHub).

## One workstream per worktree

To work two workstreams at once, run each one in its own worktree, with its
own terminal and agent session (the main session and its subagents work in
that folder only):

- At `/drwil-start`, answer "New worktree" to the `drwil-worktree` question:
  the folder `../<repository>-<brief>` and its branch `workstream/<brief>`
  are created from the up-to-date main branch, the install command declared
  as `worktreeInstall` in `.drwil/ia-first.json` is run there (none declared:
  the output says to install the dependencies), and the workstream starts
  there. Open a terminal in that folder and start the agent; the folder you
  answered from is unchanged.
- A brief already active in another worktree cannot be started again: the
  refusal gives that folder's path.
- The state reminder of each folder says the others are worktrees of the same
  repository: never modify another folder's files.
- Expect conflicts at closing on the index (`docs/projects/pending.md`) and
  on `docs/projects/journal.md`, which every workstream edits: merge the main
  branch into yours (`node .githooks/close.mjs --check` reports a branch
  behind it and gives the command) and resolve them by hand before the
  closing; the closing is not refused for being late.
- After the merge, clean up by hand, nothing does it for you:
  `git worktree remove ../<repository>-<brief>`, then `git worktree prune`;
  the merged branch is deleted at the next start.

## Closing

- Once the pull/merge request is merged (merge or squash, project's
  choice, the kit doesn't impose anything here): the branch is deleted
  locally at the next start (below); brief
  fully finished: condense it (below); otherwise update its Status (increments
  done, increments left). Only the marker `**Status**: done on YYYY-MM-DD` says
  a brief is finished, and a brief carrying it cannot stay: check-docs
  refuses it, and so does going back to Scoping while it is there.
- If the merge is done by someone other than the author of the work
  (human review), that person closes the brief at merge time — not
  before.
- Merge authorised at start: the agent schedules `gh pr merge --auto
  --merge` once the closing commit is pushed; the host merges after the
  required checks, with no second gesture.
- If the whole brief is now finished (not just one increment among several of a
  brief that otherwise stays open): summarize the brief into a short entry
  in `docs/projects/journal.md` (date, title, key decisions, closing
  commit/PR, and the closing proof of the workstream — the entry cites it),
  then run `node .githooks/close.mjs` (in Closing): it writes the closing
  proof (below), deletes
  the brief file, removes the matching item from `docs/projects/pending.md`
  (don't check it: the journal becomes the closing source, the index only
  lists remaining work; a past entry is read by itself:
  `node .githooks/glossary.mjs --journal <brief>`, never the whole journal) and records the consumption. It stages, commits,
  pushes and merges nothing. Nothing is lost — the full history stays in
  git (`git log --follow -- docs/projects/<brief>.md`).
- The closing writes the workstream's proof first, a versioned file in
  `docs/preuves/` (`dirs.proofs` setting): every acceptance criterion with
  its contract, that contract's checks, the verified commit and the
  fingerprint of the files it covers, copied from the latest `drwil verify
  --evidence` evidence. Without an evidence of the verified commit, with a
  verdict that does not pass, or if the file cannot be written, the closing
  is refused and nothing is deleted: renew the evidence (`npx drwil verify
  --evidence`) after the closing commit, then close. `drwil verify` replays
  these criteria afterwards and fails, naming them, when a proof has
  disappeared — a row is never deleted to make a verdict green.
- Before the closing commit, the same command reports what would make the
  closing fail: a criterion still « contract to create » (with the ID to
  write), places citing the brief that would go dead, backticked file names
  without a path. `node .githooks/close.mjs --check` gives that report
  alone, at any time; leaving Build for Evidence gives it too, while code
  can still change. Code cites the contract it serves, never a workstream
  brief: check-docs refuses such a citation in a changed code file.
- Local branches already merged into the main branch are deleted when the
  next workstream starts — never an unmerged branch, the current one or
  the main branch. A branch is merged when git sees it in the main branch
  (classic merge) or when a merged GitHub pull request carries its last
  commit, squash included (asked to `gh`); the resume of a detour
  (`node .githooks/detour.mjs resume`) uses the same test. Without `gh`
  reachable (absent, not logged in, another forge), only a classic merge
  is recognised, and the start or the resume says so: a branch merged by
  squash is then kept, to delete by hand.
- Keep what the user reads current, in the closing commit: a workstream
  that changes what the user sees updates the `README.md` and adds its line
  under « Unreleased » in `CHANGELOG.md` (if present); one
  that changes the structure (folder or module, layer, stack) updates the
  map `docs/architecture.md` — drwil maintains it, and its check reports
  any code folder it does not cite yet. These three are written in the
  `docLang` language (absent: the project language).
- Record the workstream's consumption, in the closing commit (done by
  `node .githooks/close.mjs`; by hand:
  `node .githooks/consumption.mjs --record <brief>`), which appends its line to
  `.drwil/usage.jsonl` (if present) (tokens, models, active time, read from Claude
  Code's session logs; never twice for the same workstream). Another AI
  tool: estimate by hand (`docs/recipes/track-consumption-per-increment.md`).
- Close the intent loop, in the closing commit: each limit left untreated
  becomes an intent brief (`docs/intentions/`, one per limit, never a
  workstream brief), listed in `docs/projects/pending.md` as `[decision]`
  and cited from the journal's « Known limits »; the original intent (the
  brief's `**Intent**` field) is updated, or summarised in the journal and
  removed when fully covered. A closing commit that deletes the brief
  without touching its intent gets a warning.
- Defect revealed by the PR after the closing commit, before the merge (red
  CI, forge code analysis): never a workaround nor routine maintenance —
  `node .githooks/state.mjs reopen` (in CLOSING, on the workstream's branch,
  clean worktree, the PR open: gh must answer, otherwise refused) cancels
  the scheduled automatic merge first (`gh pr merge --disable-auto`: a
  branch whose merge is scheduled receives no push), restores the brief as
  it was before the commit that deleted it, in a commit touching only the
  brief, and moves back to BUILD on it, keeping the merge authorisation and
  the queue decided at start. Widen its scope, if needed, in a separate
  commit; commit the fix and push normally (no history rewritten). A fix
  that changes the rules (an existing contract modified or removed, hook,
  threshold, gate, CI) waits for the human's yes, like a detour
  (`node .githooks/detour.mjs allow`, interactive terminal). Then Evidence,
  Verify and Closing as the first time: update the workstream's journal
  entry (a second entry citing the same proof makes the closing fail),
  `node .githooks/close.mjs` rewrites the proof on the new verified commit
  and replaces the consumption line with the new total; schedule the merge
  again if it was authorised. A PR already merged or closed is refused: a
  new workstream, or routine maintenance.
- Queue (decided at start, several briefs ticked in the `/drwil-start` poll): once the
  closing commit is pushed and the merge scheduled, go back to Scoping, wait until the PR is
  merged, update the main branch, then start the next brief yourself:
  `node .githooks/state.mjs next` (allowed to the agent for the head of the queue only). Stop
  at the first workstream that gets stuck (human attestation, FAIL not fixed, red CI, decision
  to make) and say why; `node .githooks/state.mjs queue stop` drops what is left.
- Active governance: the brief can be deleted during CLOSING (the state
  tolerates its absence in that activity only), then go back to Scoping
  (`node .githooks/state.mjs move SCOPING`). The next workstream starts in a
  new session (Claude Code: `/clear`; another tool: a new conversation): a
  fresh context does not read the closed workstream again; a queue keeps its
  subagent per workstream.
- If the brief still has open points elsewhere (multi-increment project), don't
  condense anything: check the matching box in the index as before, the
  brief stays active until it's fully finished.

## What the kit checks (QUA-017)

- `.githooks/run-checks.mjs` (so `pre-commit` and `pre-push`) refuses to
  commit or push directly to the main branch (`master`/`main`), including
  a repository's very first commit: no more bootstrap exception. No
  setting disables this check.
- This check never runs in CI: it only applies locally, at commit/push
  time, not when CI replays an already-made push (a merge landing on the
  main branch is legitimate).
- If the main branch CI breaks anyway (merge landed too fast,
  flakiness...): a dedicated CI job (`alert-if-ci-broken`, GitHub and
  GitLab) automatically opens an issue linking to the failed run — only
  on the main branch, only when the checks failed, without creating a
  duplicate if an equivalent issue is already open. This job alerts, it
  does not fix: the fix remains a separate workstream (dedicated brief).

## What the kit does not do

- No automatic check verifies the branch **name**
  (`workstream/<brief-slug>` stays a documented convention, not enforced)
  or requires a pull/merge request to exist before a commit on a
  non-main branch.
- The kit does not open or merge pull/merge requests on the human's
  behalf (no GitHub/GitLab API calls): only description templates are
  provided.
