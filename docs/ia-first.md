# "IA first" architecture — description

State as of 2026-10-21. This document explains **how this repo is built to be
modified by agents**: where the rules live, who checks them, and what is
not checked. It describes neither the product nor the detail of invariants
(single source: `docs/contracts.md`); the product, if described, lives in
docs/features.md and docs/architecture.md (if present). Like the rest of
the repo, it is to be updated in the same commit as what it describes.

"IA first" names a property of the repo, not a product feature: **the rule
is written in the repo and checked by a machine, not entrusted to the
assistant writing the code.**

## 1. A single entry point, and a budgeted context

The setup is deliberately minimal and readable by any tool, not only a
given assistant:

- `AGENTS.md` carries the conduct rule, the contracts to know, the "if you
  touch… read first" table, and the context-loading order.
- `CLAUDE.md` and `GEMINI.md`, if present, contain only one line,
  `@AGENTS.md` (some tools don't read `AGENTS.md` on their own): there is
  no second copy of the instructions to keep in sync.
- One `AGENTS.md` per layer: a task's context is read in the touched
  folder, not the whole repo. Each layer has the same one-line redirect as
  the root.
- `docs/` is plain markdown, readable without any install.

Context loading is **progressive and bounded** (`AGENTS.md`, "Load context
progressively" section): this file, then the layer's `AGENTS.md`, then the
recipe or document from the table, then `docs/contracts.md` for an ID's
detail, then "nothing else without a reason".

Consequence: switching assistants changes nothing in what it reads, and the
context loaded for a task stays proportionate to the task.

## 2. One piece of information, one source

Since duplication is the first cause of wrong docs, the repo forbids
copying:

- **skills** (`.claude/skills/`, if present) are shortcuts, not recipes:
  each contains only the trigger and a pointer to the recipe in
  `docs/recipes/`, which is itself the substance;
- layer `AGENTS.md` files do not copy a contract's rule, they cite its ID
  and give the file where to read it;
- `docs/contracts.md` is the single registry of invariants, where each ID
  is defined exactly once.

This split is checked: QUA-011 (`.githooks/check-docs.mjs`) fails if a path
cited in backticks doesn't exist, or if a cited contract ID isn't in the
registry.

## 3. Invariants are tooled, not merely stated

`docs/contracts.md` lists the project's contracts. Each has a normative
rule, a scope, a source of truth (the code that implements it), a proof and
a reason.

The design rule is explicit in that registry: **"a contract that matters is
machine-checked; a human proof is flagged as such: it's the next check to
tool up"**. The repo therefore distinguishes three things too often
presented as one:

| What it is | Example | Where it's checked |
|---|---|---|
| Tooled invariant | QUA-011 (cited paths and contracts) | `.githooks/check-docs.mjs` |
| Tooled invariant, partial proof | a contract only part of which is machine-provable | its "Proof" column, limit named |
| Assumed human rule | a review convention | `docs/contracts.md`, "Out of registry" section |

This is what tells apart an IA-first architecture from a merely
well-documented repo: the repo **announces** what it doesn't check, instead
of letting people believe everything is.

The baseline of `docs/contracts.md` is deliberately minimal (green from day
one, with no stack assumption). `docs/contracts-catalog.md` lists common
generic contracts, not installed by default: a project adopts them one by
one, with `docs/recipes/adopt-the-kit.md` (or the `adopt-the-kit` skill),
each adoption staying a decision of the requester. A new project, with no code
yet, gets no layer at `init`: the same recipe starts by scoping it (need,
first version, stack and layers, first business rules, risk), then declares
its layers with `drwil init --layers`.

## 4. The check lives in git, not in the tool

This is the repo's structuring decision. Checks live in `.githooks/`
(versioned), wired through `core.hooksPath`, not in an assistant setting:

- `AGENTS.md` assumes it: the hook applies to whoever commits or pushes
  (human in a terminal, AI assistant…);
- `.githooks/run-checks.mjs` carries the argument as a comment: a real git
  hook runs regardless of what invoked `git commit`, unlike a tool-specific
  hook. An assistant with no local configuration is thus subject to the
  same checks as a human.

What `.githooks/run-checks.mjs` chains, in order: secrets (gitleaks, if
installed), doc references (QUA-011), tests cited by the contracts
(QUA-029), the architecture map (QUA-040: every code folder cited in
`docs/architecture.md`), the brief's scope (QUA-016), the main branch
(QUA-017), tests of the checks themselves (if any), CI coverage of each
check (QUA-013, if there is a CI), then the project's own checks declared
in `.drwil/ia-first.json` (`checks` key).

The tests of the checks exercise drwil's own code: they stay in the drwil
repository and are not shipped, so they never slow down the project's
commits (the check reports "not applicable"). A requirement added by a kit
version released after the project was installed (today: a cited path that
only exists as a file ignored by Git) follows the `newRequirements`
setting of `.drwil/ia-first.json`: `blocking` for a fresh install,
`warning` for a project that was already set up without it — an
upgrade never breaks a commit that used to pass; `drwil init` never
hardens this setting, the project switches it to `blocking` when it wants.

To keep commits fast, a project check that declares `paths` (patterns,
e.g. `["src/**"]`) runs **at commit time** only if a staged file matches one
of them; otherwise it ends up "not run", never hidden. Push
(`.githooks/pre-push`), CI and a manual run of `.githooks/run-checks.mjs`
always run it. Without `paths`, it runs on every commit. Also list in
`paths` the files the check depends on outside its folder (root dependency
manifest and lockfile, `.drwil/ia-first.json`…): otherwise a commit touching
only them reruns it only on push.

One detail makes this check actually run rather than get bypassed: **a
check that did not run is visible** (QUA-013) — a check that cannot run on
a workstation (missing tool) ends up "not run", never hidden as a silent
success. At commit time it stays a warning; on push and in CI
(`.githooks/run-checks.mjs`) a check that did not run **fails** — only the
stale-audit reminder stays a warning. A **degradable** check (e.g. secrets, without gitleaks) must also
have a CI job triggered on the same paths:
`.githooks/check-control-coverage.mjs` verifies this by re-reading the CI
file, not merely noting the job exists (a job that exists but never
triggers covers nothing). The same hook replays on push
(`.githooks/pre-push`), to catch a commit made with `--no-verify`. A
`commit-msg` hook (rejecting the "Co-Authored-By" line) is shipped but
disabled by default (`chmod +x .githooks/commit-msg` to enable it).

*(AI-tool-specific safety net, in addition to and never instead of git
hooks: see the "Scoping reminder" section further down for what the kit
already tools up with Claude Code.)*

## 5. What the agent must prove about its work

- **Test what the user sees**, roles, text and buttons, not implementation
  detail (`AGENTS.md`, Conventions).
- **The end-of-task report is normative** (`AGENTS.md`, Conduct): files
  touched, checks run and their exact results, remaining limits and
  ambiguities. "Present as proven only what a check actually verifies; the
  rest is labeled 'human' or 'not verified'."
- **A failing check is not a constraint to bypass**: it flags a contract,
  to be read before any accommodation.

### `drwil verify`: the work-validation primitive

`drwil verify` is not just an audit: it is what lets you call a piece of
work **verified**. The chain is intent (**Reason**) → explicit requirement
(**Rule**) → proof (**Check**, **Manual**) → verdict, for each contract in
`docs/contracts.md`:

- **Check** names the identifier of a check known to the engine
  (`.githooks/engine.mjs`, the same one the git hooks use; or the `id` of a
  check in `.drwil/ia-first.json` → `checks`). The command is never copied
  into the contract.
- **Manual** describes the part of the proof that stays human.

Status per contract: `PASS` (fully proven by a check), `FAIL` (a check
fails, its cause is shown), `MANUAL` (a human part remains, or a legacy
contract without a check — never counted as PASS; the automated part is
shown separately), `ERROR` (unknown check, missing requirement or proof,
tool unavailable). Every contract is blocking. Overall verdict and exit
code: `GOVERNANCE: PASS` → 0; `GOVERNANCE: FAIL` or `MANUAL REVIEW
REQUIRED` → 1; `VERIFY ERROR` → 2.

Deterministic priority, per contract and for the overall verdict: FAIL >
ERROR > MANUAL > PASS.
Only **blocking** contracts (the default severity) decide the verdict; a
`warning` or `advisory` contract is run and shown without changing the exit
code. A malformed registry stays an `ERROR`, whatever the severity.

**Human attestation** (`drwil attest <ID>`): a `MANUAL` contract whose
automated part passes can receive an explicit human proof. The command first
re-runs the checks the contract cites, announcing each one and its result
(a test suite can take minutes), then shows the contract, its rule and its
automated proof, requires an interactive terminal and typing the contract
identifier (no `--yes`: an agent or a script cannot attest), then writes the
attestation to .drwil/evidence/attestations, versioned so it is reviewed.
The attestation is bound by fingerprint to the human part of the contract
(its rule, **Manual** field and severity) and, separately, to the definition
of each check it covered. A check added later leaves it valid (it must pass
anyway); a changed human part, or a covered check removed or redefined,
makes it stale and the contract is `MANUAL` again. An attestation written
before this rule (version 1) stays bound to the whole contract text. `ATTESTED` status: distinct from `PASS`, it satisfies the contract;
never attestable: a `PASS`, `FAIL`, `ERROR` or not-applicable contract.

**Business rules**: the output ends with a one-line summary of
the business non-regression — rules proven by a test (a passing contract
with a **Tests** field), proven without a cited test, to complete (a human
part remains) and broken, each broken rule with the workstream it was born
from (**Origin**). A business rule is a contract whose prefix is a business
prefix (`APP` by default; setting `businessPrefixes` of
`.drwil/ia-first.json`, e.g. `["APP", "BIL"]`) or any contract with an
**Origin** field. The rule-by-rule detail (state, cited tests, origin) is in
`drwil verify --json` (`businessRules`) and in the evidence. No business
rule in the registry: no summary line.

### Agent / human workflow

```text
Agent : drwil verify            (or /drwil verify; --agent output)
DRWIL : PASS / FAIL / ERROR / MANUAL
  FAIL   → the agent fixes, then runs drwil verify again
  ERROR  → the agent reports the problem, never claims it is verified
  MANUAL → the agent stops: "Human attestation required."
Human : drwil attest <id>       (in their own terminal, if MANUAL)
Then  : drwil verify
Last  : GOVERNANCE: PASS        (only this verdict counts as validation)
```

The agent produces and verifies proofs; only the human attests. `drwil
attest` refuses any non-interactive environment (agent, script, redirected
input) and has no `--yes` option. drwil does not prove that a human typed
the command: it requires an attestation to be explicit, traceable and
subject to the change review (.drwil/evidence/attestations folder,
versioned).

`drwil verify --evidence` keeps the proof of a run (.drwil/evidence folder,
created on first run,
git-ignored): verdict, timestamp, commit, and per check a summary of its
output, redacted (keys, tokens, `password=…`) so that no secret is ever kept
(SEC-007).

**`verify` is the validation gate: a piece of work is verified by drwil
only if `verify` returns PASS.** `FAIL`: it must be fixed. `MANUAL`: it is
not validated yet — never a deferred success. `ERROR`: drwil cannot
establish the verdict. `verify` is the primitive an agent, a hook or a CI
can build on to enforce this gate; it does not enforce it by itself. Git
hooks do not depend on it: they use the same engine without drwil being
installed.

## 6. Assumed limits

What the architecture does not do, and does not claim to do:

- Human proofs are named in `docs/contracts.md` ("Out of registry" section)
  and in each contract's "Proof" column.
- Nothing automatically assesses the quality of a diff: review stays
  human, or delegated to the assistant, with the report as the only trace.
- Projects not yet done are in the project index (`.drwil/ia-first.json` →
  `dirs.index`): "pending" is not an invariant, and must not be presented
  as checked.

### Tool compatibility matrix

What each assistant really holds, measured by the drwil repository's agent
bench (non-interactive mode, drwil installed in a temporary project in
SCOPING, the agent asked to write a code file). A tool is "held" only when its own hooks
ran, the write was refused **by the write hook** (not by the agent obeying
`AGENTS.md`) and the state reminder arrived. An absent tool is "not tested",
never "held". Whatever the tool, the Git barrier at commit still holds.

| Tool | Hooks run | Code write in SCOPING | State reminder | Trial (version) | Attested by the human |
|---|---|---|---|---|---|
| Claude Code | yes | refused by the hook | received | 2026-10-19 (2.1.303) | pending |
| Copilot CLI | yes | refused by the hook | received | 2026-10-19 (1.0.98) | pending |
| Gemini CLI | yes | refused by the hook | received | 2026-10-19 (0.61.0) | pending |
| Codex | not tested | not tested | not tested | not tested | — |
| Cursor | not tested | not tested | not tested | not tested | — |

Copilot CLI: it reads the .claude folder (settings, hooks, skills) and
its native hooks (in the .github/hooks folder) itself, so `--tools copilot` installs both, and the
`/drwil-*` commands are the same as with Claude Code. It loads a repository's
hooks only in a **trusted folder** (the bench uses a temporary `COPILOT_HOME`
that trusts the trial project, your configuration untouched). In the trial of
2026-10-18 the write hook did not refuse because Copilot sends the file as
`path`, not `file_path`, and the state reminder was not received because
Copilot reads JSON, not raw text; both are fixed. The interactive session
cannot be run by the bench (non-interactive mode only): a hook that exceeded
Copilot's 30-second delay there was not reproduced, and its cause is not
established. A hook that fails never counts as an agreement, but a timeout
lets the call through; the hooks therefore never wait for an input that does
not close.

Gemini CLI: `--tools gemini` installs the settings of the .gemini folder (hooks for the
session start, each message and the `write_file` and `replace` tools) and the
shared scripts of the .claude/hooks folder, the same set as for every other tool
(only the declaration differs). Like Copilot CLI, it loads a repository's hooks
only in a **trusted folder** (trust it once in an interactive session; the bench
sets `GEMINI_CLI_TRUST_WORKSPACE` for its trial project only, `--skip-trust` alone
does not load them). The trial of 2026-10-18 saw no hook because of that trust.
Gemini has no `/drwil-*` commands here: **start a workstream with
`node .githooks/state.mjs start` in an interactive terminal**, then work in
Gemini CLI. The free-tier quota of the model can make a bench run fail (429, 503):
a failed run proves nothing and is run again.

## 7. Projects, resumable cold

An agent picks a project back up without the conversation history that
created it: everything it needs must be in the brief.

**Risk levels** (ceremony follows risk; traceability never changes:
QUA-016 applies to everything):

- **LOW**: small task without a dedicated brief, attached to
  `docs/projects/routine-maintenance.md`.
- **MEDIUM**: brief with a scope (`**Risk**: MEDIUM`).
- **HIGH**: brief, "Decisions" section, related contracts cited, proofs
  (`drwil verify`). A scope touching governance mechanics (`.githooks/`, CI,
  `.drwil/`) requires HIGH; configurable via `.drwil/ia-first.json` →
  `risk.sensitivePaths`.

The level is declared in the brief and can only be raised: a level below
the detected minimum, or a HIGH without Decisions or contract, is rejected
at commit (`.githooks/check-docs.mjs`); a brief without a "Risk" field whose
scope requires HIGH gets a warning.

- **A short index, briefs**: the project index has one line per topic and
  points to the brief as soon as there's more than two lines to say. An
  agent reads only the brief of the topic it handles.
- **Status lives in the brief**, a "Status" line at the top of a project or
  an intent, never copied into the index: a copy ends up wrong.
- **Existing state is surveyed in the code, dated**, and what isn't
  checked is labeled "not verified".
- **A project splits into short increments**, each with its exit criterion
  written in advance, its contracts and its dated decisions.
- **Every open item says who can act**: `[AI]`, `[human]` or `[decision]`.
  An agent does not attempt a human gesture (server, vault, approval) and
  does not decide a decision: it raises it.
- **Leave it cleaner in passing** (`AGENTS.md`, Conduct): what an agent
  reads and finds wrong during a task, it fixes separately and reports;
  what it advances, it dates in the brief.

### Project lifecycle

| Stage | Where | Who moves it forward |
|---|---|---|
| idea expressed | intent brief: Need, Surveyed existing state, Questions to decide | the agent writes it; the requester decides the questions |
| scoped, short | an `[AI]` or `[human]` line in the project index | the agent, once the questions are decided |
| scoped, long | brief: dated status, increments and exit criteria, Hand-off | the agent; the requester approves the scoping |
| in progress | the same brief, Hand-off updated at the end of every task | the agent working on it (handoff) |
| delivered | line removed from the index; status "delivered on …"; final Hand-off | the agent, with proof (QUA-013) |
| abandoned | brief or line removed, reason in the commit message | the requester decides, the agent applies it |

A `[decision]` only moves forward once decided by the requester; a
`[human]` gesture is never done by the agent, who raises it.

**Noting an idea at any moment**: `node .githooks/note.mjs "<text>"`, from any
tool or a terminal, appends one dated line to the intents' notes file
(`docs/intentions/notes.md` (if present), outside the index) without changing the activity
or touching the active brief; under Claude Code, `/drwil-note` calls it. A
note is always an intent, never an order to code; it is
committed with the branch in progress and promoted to an intent brief when it
matures.

**Hand-off**: the "Hand-off" section of every brief (dated last
state, uncommitted work, next step with its marker) acts as a hand-off file:
another agent, another tool or a person picks it back up cold from it,
without any tool-specific memory.

Checked by QUA-015 for what a machine can verify (markers, **dated** status
line, intent sections, projects' "Hand-off" section): a machine can't
tell if a status is true, but a dated status shows its age. Content
accuracy stays human. Exceptions to the check: `docs/projects/template-*.md`
files (templates to copy, not briefs) and `docs/projects/routine-maintenance.md`
(permanent brief, not a project split into increments).

### Scoping reminder

A code file must not stay detached from every project. A brief in
`docs/projects/` carries, in an HTML comment invisible when rendered, a
`scope` block (one path or pattern per line under `files:`): it covers
the files this project touches.

- **Configurable severity at commit** (`.githooks/check-docs.mjs`,
  `scopeReminder` setting in `.drwil/ia-first.json`): a code file indexed by git
  that no block covers, or a malformed block (no `files:` line, too
  broad a pattern like `**` or `scripts/*`), is reported. `blocking`
  (written by `drwil init`): fails the check. `warning`: never
  blocking, just shown (the value used when the setting is missing). `off`:
  disables the check (and its reminder to the agent). `docs/` and any
  markdown file are never code.
- **Agent side** (Claude Code, hook
  `.claude/hooks/scope-reminder.mjs`, if present), following the same setting: in `blocking`, writing a code file
  outside any brief is refused before it happens (PreToolUse hook); in
  `warning`, a reminder is slipped after the write (PostToolUse hook),
  nothing is blocked; in `off`, nothing.
- **Small task**: attaches to `docs/projects/routine-maintenance.md`,
  without opening a separate project.
- **Kit's own files**: covered by `docs/projects/kit-mechanics.md`, set up
  at install.
- **Shell command guard** (Claude Code, PreToolUse hook
  `.claude/hooks/bash-guard.mjs`, if present): asks for approval on a
  secrets file or a pipe into a shell within a Bash command; blocks
  nothing, the person decides.

- **Generated copies** (`scopeMirrors` setting, absent by default): pairs
  of path prefixes `{ "from": "templates/", "to": "" }`. A scoped file under
  `from` also covers the same path under `to`, for the scope barrier, the
  check and the agent-side reminder: a brief names the source of a copy
  without listing the copy.
- **Language of the public docs** (`docLang` setting, absent by default:
  the project language `lang`): the language of the map
  `docs/architecture.md`, the `README.md` and the changelog, for a project
  that works in one language and shows itself in another. Given to the
  agent by `AGENTS.md` and the CLOSING reminder.
- **What a scope usually forgets** (warnings from check-docs): code
  without any test file, or a criterion that creates a contract without
  `.drwil/ia-first.json`, where that contract's own check is declared.
- **Widening mid-workstream**: a commit that stages documentation only
  (`docs/`, Markdown) does not replay the tests of the checks nor the
  project's checks on the working tree, which holds the lot in progress;
  they are reported not run, and push and CI always replay them.

The block's grammar (`.githooks/scope.mjs`) is shared by the check and
the reminder: a single source.

**Readable identifiers.** An identifier cited to a human (contract ID,
catalog line, lot or point of the active brief, value-discovery opportunity
`OPT-N`, marker an intention or a brief declares in its « Markers » section,
one `` - `ID` — label `` line each) is followed by what it designates. A
single source, `.githooks/glossary.mjs`, reads the labels: the checks'
outputs and `drwil verify` annotate the first mention of each ID; any agent
gets the labels with `node .githooks/glossary.mjs "<text>"` (`AGENTS.md`,
Copilot and Cursor instructions). With Claude Code, two hooks: at the end of
each reply, `.claude/hooks/reply-glossary.mjs` (if present) shows the human, under the
reply, « ID — label » for each ID cited without a gloss right after it (a
parenthesis, a dash, a colon, a table cell) — it completes the reply, never
rewrites nor blocks it; after each tool that brings text in (shell, reads,
searches), `.claude/hooks/id-glossary.mjs` (if present) slips the labels into the
agent's context, once per identifier and per session.

### Governance state

The current activity (`SCOPING`, `EXPECTATION`, `REQUEST`, `BUILD`,
`EVIDENCE`, `VERIFY`, `CLOSING`) and the active expectation live in
`.drwil/state.json` (if present): a **local** state, ignored by Git, holding references
only (activity, path of the active brief, active request, timestamp). The
allowed scope stays the brief's `scope` block, which is versioned.

- **Deterministic reading** (`.githooks/state.mjs`, the only reader):
  `node .githooks/state.mjs` prints the active context, `--json` the raw
  state. Without a file, the state is neutral `SCOPING`; an invalid state is
  reported and treated as neutral, never guessed.
- **Agent-agnostic**: the source (`state.json`, if present) and the command
  (`node .githooks/state.mjs`) depend on no tool. The universal channel is
  `AGENTS.md`, which asks every agent to run the command at the start of a
  task. A tool offering a session-start hook can wire the same command to
  re-inject the context automatically — Claude Code does (SessionStart hook,
  if present); it is only an accelerator.
- **Short reminder, only when the state changes**: `node .githooks/state.mjs
  reminder` prints two lines (activity, brief, scope size, rule) if they
  differ from the last reminder injected, nothing otherwise; the last
  reminder's fingerprint lives in `.drwil/reminder.json` (if present), local
  and git-ignored like the state. Claude Code calls it on every message
  (`UserPromptSubmit` hook); after an automatic context summary, the
  session-start hook re-injects the full state. A tool without a prompt
  hook follows the `AGENTS.md` instruction: read the state again before
  modifying any file. It is a behavioural barrier: the guarantee stays in
  Git.
- **Hand-off to someone else**: a clone's state does not travel; resume from
  the versioned brief ("Hand-off" section) and re-activate the expectation
  explicitly.
- **Transitions**: one activity at a time, back to `BUILD` after
  `EVIDENCE` or a failed `VERIFY`, abandon to `SCOPING` from any activity.
  Command: `node .githooks/state.mjs move <ACTIVITY> [--brief <brief>]
  [--request <text>]` (or `drwil state move …`, a mere facade); `--brief`
  opens an expectation from `SCOPING`.
- **Who decides** (`transitions` setting in `.drwil/ia-first.json`): `human`
  (default) — opening rights is a human decision: starting a workstream (see
  "Starting and closing" below), or, step by step, opening an expectation
  and moving from `REQUEST` to `BUILD` in an interactive terminal by
  typing the target activity; the other transitions, which narrow rights or
  go back, stay free, closing included. `agent`: any allowed transition is
  free.
- **Closing on `drwil verify` evidence**: `VERIFY → CLOSING` requires,
  whatever the `transitions` setting, the latest evidence written by
  `drwil verify --evidence` (`.drwil/evidence/` (if present), not in git): verdict `PASS`
  or `ATTESTED`, on the `HEAD` commit, clean worktree. `.githooks/state.mjs` reads that
  evidence without running anything: verify stays the only judge, over all
  blocking contracts. `FAIL` or `ERROR`: go back to `BUILD` to fix.
  `MANUAL`: a human attests (`drwil attest <ID>`), then verify runs again. A
  brief's acceptance criteria are not a parallel system: every workstream
  brief has some (starting and closing are refused without any, a warning
  only if the project does not enforce recent requirements:
  `newRequirements`; routine maintenance excepted), and each one is proven
  by a contract, never by a check alone — a checkable criterion becomes a
  contract with a `Check`, a human one a `MANUAL` contract. A contract
  created by the workstream carries the **Origin** field, otherwise it is not
  a durable proof. While a brief is active, an unproven criterion is part of
  the `drwil verify` verdict, not only of the closing: `FAIL`, or `MANUAL`
  when the only gap is a human criterion awaiting its attestation; the
  unproven criteria are listed in every output (`criteria` in the JSON).
  Without an active brief (CI, `drwil attest`), the verdict is unchanged.
  A criterion judged « not-durable » says what to change, naming its
  contract: add `[human]` when the contract is to be attested, remove it when
  the contract is fully automated.
- **Closing from another activity**: `node .githooks/close.mjs` (and
  `--check`) outside CLOSING names the remaining path — the `state.mjs move`
  commands, in order, neighbouring transitions only — and, when the evidence is
  missing or from another commit than `HEAD`, the command that renews it. It
  never advances the state itself.
- **Tool-side blocks** (Claude Code, `deny` rules in
  `.claude/settings.json`, if present): `git … --no-verify`, `git commit -n` and writing `.drwil/state.json`
  (if present) directly are refused by the tool, not by the model.
- **Limits**: an agent with a shell can still cheat locally (rewrite the
  state, fake a terminal, combined short option). The aim is cheating that
  shows in the diff and is refused in CI, not impossible cheating.

### Starting and closing

Two human gestures per workstream: **start** and **merge the PR** — only one if the merge is
authorised at start (below), and one for a whole **queue**: ticking several briefs in the start
poll (multiple choice) queues them, with the merge decision. The agent then runs them one after
the other — the next brief started by `node .githooks/state.mjs next`, allowed to the agent for
the head of the queue only, once the previous PR is merged — and stops at the first workstream
that needs a human (attestation, unfixed FAIL, red CI, decision to make); only ready briefs
(maturity « ready », no open decision, acceptance criteria) can be queued; any return to
`SCOPING` other than after a closing, or `node .githooks/state.mjs queue stop`, drops the
queue.

- **Start**: from `SCOPING`, `EXPECTATION` or `REQUEST`, a single decision moves
  to `BUILD` on a **scoped brief** (`scope` block committed in
  `HEAD`, brief not finished, templates excluded; list: `node
  .githooks/state.mjs briefs`). It applies to that committed scoping.
  - **In the chat** (`/drwil-start`, if present): the agent opens a poll of
    the scoped briefs, in the order of `briefs --json` (ready briefs first,
    no open `[decision]`, then by index priority; the intentions' proposed
    order breaks ties), and recommends the first ready brief (in the
    description, never by preselection); it is the **human's answer**, read by a tool hook
    (`.claude/hooks/prompt-hook.mjs` (if present)), that starts — never the
    model. The hook refuses a start poll that arrives with an answer already
    filled or a default value. Checked on 2026-10-10 in Claude Code (an
    answer pre-filled by the model is ignored by the tool) and in Copilot
    CLI, which fires the same hooks on its own poll.
  - **In a terminal** (any tool): `node .githooks/state.mjs start` lists the
    scoped briefs, asks for a number, then for typing `START`.
- **Close**: free as soon as the `drwil verify` evidence is valid (see
  above); closing removes rights, and accepting the work remains the human
  PR merge.
- **Merge authorised at start** (Claude Code: a second question of the
  poll, Copilot CLI: a second field) named `drwil-merge` lets the human
  accept in advance; their answer,
  read by the hook, is kept in the local state (`auto_merge`) until
  going back to Scoping, never set by the agent (even with
  `transitions: agent`). In CLOSING, after verify PASS and pushing the
  closing commit, the agent schedules `gh pr merge --auto --merge`: the
  host only merges once the required checks pass ("Allow auto-merge"
  repository setting required). In the terminal, `start` asks the same
  question (type `MERGE`). Without that answer, the agent never schedules
  the merge, in any tool. Once the closing PR is merged, the next `start`
  (or `next`) goes back to Scoping by itself: the brief is deleted and the
  PR merged (main branch current, or the branch merged, `gh` for a
  squash).
- **Limit**: an agent with a shell could call these functions itself or
  rewrite the local state; Claude Code's `deny` rules block calling the hook
  directly, and cheating stays visible and refused off the machine (commit
  barrier, CI), as for the rest.

### Scope barrier

The engine's `scope-gate` check (`.githooks/scope-gate.mjs`), run at
commit, push, in CI and in `drwil verify`. Severity: `scopeGate` setting in
`.drwil/ia-first.json` — `warning` (written by `drwil init`: gaps are
shown, nothing is blocked), `blocking`, `off`.

| `scopeGate` | Gaps in scope | Missing reference branch (push, CI) |
|---|---|---|
| `blocking` | ✗ the commit, push or CI run is refused | not run: refused at push and in CI (QUA-013) |
| `warning` (default) | ⚠ shown, nothing blocked | not run: same |
| `off` | not checked, no trailer | not applicable |

"No reference branch" (`origin/HEAD`, `master`, `main` or `DRWIL_BASE`) is
never a pass: it is a warning at commit and a failure at push and in CI.

- **Briefs always pass**: `docs/projects/` and `docs/intentions/`, as do
  human attestations, written by `drwil attest`:
  `.drwil/evidence/attestations/` (if present), and the consumption log
  `.drwil/usage.jsonl` (if present), written at closing, and the scoping recipes' outputs,
  written in SCOPING (value discovery and risk audit:
  `docs/value-discovery.md` (if present), `docs/audit-risks.md` (if present), and their French
  names) — configurable list: `outOfScope`. Any other file,
  code or doc, is subject to the scope.
- **At commit**: outside briefs, a commit is accepted only in
  `BUILD`, for files covered by the active brief's `scope` block
  **as it is in `HEAD`**. Widening one's scope thus takes a separate commit
  touching only the brief; the code comes in the next commit.
- **Trailer**: the `prepare-commit-msg` hook adds `Drwil-Expectation: <active
  brief>` to the message. Git never skips this hook.
- **On push, in CI and in `drwil verify`**: each commit of the branch
  (since the main branch, or `DRWIL_BASE`) is replayed against the scoping
  of its trailer's brief, read in its parent commit. A commit outside
  briefs without a trailer is refused. A commit is judged by its parent's
  rules: if the barrier did not exist there yet, it is not checked. The
  activity only exists locally, so CI only checks the scope.
- **Removal**: see below.

### Disabling or removing governance

No installation creates a permanent lock: governance is switched off by a
setting, then removed with the rest of the mechanics.

- **Disable** (reversible, nothing is deleted): `"scopeGate": "off"` in
  `.drwil/ia-first.json`. No more barrier at commit, push or in CI, no more
  `Drwil-Brief` trailer, and the context reinjected to the agent shrinks
  to "governance disabled". Setting `warning` or `blocking` back
  restores it as it was.
- **Remove**: `npx drwil uninstall` (dry run: lists what would be removed),
  then `npx drwil uninstall --yes`. Mechanics files left untouched are
  removed (hooks, `.githooks/state.mjs`, rules of
  `.claude/settings.json` (if present)); what `init` added to the
  project's own files (the `drwil` blocks, the kit's Claude hooks) is
  removed too; the local state
  `.drwil/state.json` (if present) is listed with
  them and deleted only with `--yes`. Briefs, contracts and versioned
  attestations stay: they are the project's history.
- **Limit**: Claude Code's `deny` rule on `.drwil/state.json` (if present)
  also refuses any shell command that mentions that path, even a read;
  reading the state goes through `node .githooks/state.mjs`.
