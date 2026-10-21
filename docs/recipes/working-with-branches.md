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
  it.
- Before merging: check that the pull/merge request CI status checks
  are green (`gh pr checks <n> --watch` on GitHub).

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
  commit/PR), then run `node .githooks/close.mjs` (in Closing): it deletes
  the brief file, removes the matching item from `docs/projects/pending.md`
  (don't check it: the journal becomes the closing source, the index only
  lists remaining work) and records the consumption. It stages, commits,
  pushes and merges nothing. Nothing is lost — the full history stays in
  git (`git log --follow -- docs/projects/<brief>.md`).
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
- Queue (decided at start, several briefs ticked in the `/drwil-start` poll): once the
  closing commit is pushed and the merge scheduled, go back to Scoping, wait until the PR is
  merged, update the main branch, then start the next brief yourself:
  `node .githooks/state.mjs next` (allowed to the agent for the head of the queue only). Stop
  at the first workstream that gets stuck (human attestation, FAIL not fixed, red CI, decision
  to make) and say why; `node .githooks/state.mjs queue stop` drops what is left.
- Active governance: the brief can be deleted during CLOSING (the state
  tolerates its absence in that activity only), then go back to Scoping
  (`node .githooks/state.mjs move SCOPING`).
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
