# Recipe: fix a defect

Goal: a fixed defect never comes back unnoticed. The proof of the fix
becomes a non-regression test, attached to the contract it protects and
replayed at every commit, at push and in CI.

1. **Reproduce, then establish the root cause** (`AGENTS.md`, « Cause
   before fix »). A defect you cannot reproduce has no test yet: say so,
   do not invent one.
2. **Write the test first**, on the current code:
   - its name starts with the ID of the protected contract, if the
     registry (`docs/contracts.md`) has one: `test("QUA-016: …")`;
     otherwise, a name that states the expected behavior, without an ID;
   - it describes the expected behavior, not the fix.
3. **See it fail** for the right reason (the defect), and note it: that is
   the proof the test catches the defect. A test that passes before the fix
   proves nothing.
4. **Fix**, as close to the cause as possible; no other change in the same
   commit.
5. **See it pass**, then run all checks: `node .githooks/run-checks.mjs`.
6. **Attach**: if the test carries a contract ID, its file is listed in the
   « Tests » field of that contract (`docs/contracts.md`); the consistency
   check verifies it both ways.
7. **Say it** in the commit message and the brief: the defect, the cause,
   the test that guards it (« seen failing before the fix »).

## A defect met mid-workstream, outside the scope: the detour

Mid-workstream (BUILD), a prior defect outside the active brief's scope is
fixed by a detour, never by widening an unrelated scope, never by stopping.
The agent opens it alone: the check is at the merge of its PR.

1. `node .githooks/detour.mjs open <name> --title <defect> [--files <a,b>]`:
   the work in progress is set aside by the tool (named stash, recorded in
   the state), the branch `detour/<name>` starts from the main branch, its
   short brief is written and the state points at it. Never stash or switch
   branches by hand.
2. Scope the short brief (the fix and its test, nothing else) and commit it
   alone; then steps 1 to 7 above.
3. Before the push: a journal entry « Detour of <brief>: … » citing the
   branch `detour/<name>`, and the short brief deleted.
4. Push, open the PR titled « Detour of <brief>: … », with the merge mode
   decided at start (automatic merge authorised: schedule it).
5. Once the PR is merged: `node .githooks/detour.mjs resume`. The
   workstream's branch is rebased on the fixed main branch, the work is
   restored and the state is back in BUILD on the original brief. If a step
   fails, the command refuses without losing anything: say what blocks.

A detour that changes the rules of the game (an existing contract of the
registry modified or removed, hook, threshold, scope gate, CI) needs the
human's yes: the scope gate refuses those commits until
`node .githooks/detour.mjs allow`, run by the human in an interactive
terminal. In a queue, the subagent stops there and reports. Adding a new
contract to the registry, every existing one kept word for word, needs no
yes; in doubt, the yes is still asked.

## A reserve or a refusal from the acceptance team: sort the issue

With the release module, each reserve and each refused point of an
acceptance verdict (`.githooks/release-verdict.mjs` (if present)) is a GitHub issue that cites its
point (`vX.Y.Z-P<n>`). The agent sorts each one, and says which in the
issue:

- **a defect to fix** — something that worked, or that a criterion
  promised, does not hold (a regression): this recipe, steps 1 to 7, in
  its own workstream (or a detour if a workstream is under way);
- **an intent** — a gap, something never promised: an intent brief in
  `docs/intentions/`, listed in the index as `[décision]`; the requester
  decides whether it becomes a workstream.

A refused candidate is never promoted: once the fixes are merged, a new
candidate (`create-release.mjs --candidate`) goes back to the acceptance
team.

What is not done: disabling or weakening a test to make a fix pass (QUA-019,
no bypassing a check); storing the proof elsewhere than in the executable
test (one source).
