# Contracts (single source of invariants)

**Single source of the project's invariants.** Other documents
(`AGENTS.md`, layer `AGENTS.md` files, recipes, skills) cite a contract by
its ID and do not copy its rule.

A **contract** is an invariant of the product or the code whose violation
can be objectively observed (in the repo or at runtime). It has a stable ID
(never reassigned, even once removed), a normative rule, a scope, a source
of truth (the code that implements it), a proof and a reason. Principle:
**a contract that matters is machine-checked**; a **human** proof is
flagged as such: it's the next check to tool up.

Where the checks run: pre-commit hook (`.githooks/run-checks.mjs`, to
enable once per clone with `git config core.hooksPath .githooks`) and CI
if configured.

This registry only holds the **baseline** installed by default and the
contracts specific to this project. `docs/contracts-catalog.md` offers
other generic contracts, not yet adopted (see
`docs/recipes/adopt-the-kit.md`).

Verifiable proof (`drwil verify`, see `docs/ia-first.md`, section 5):
`**Check**` names, in backticks, the identifier of a check known to the
engine (`.githooks/engine.mjs`, or the `id` of a check in
`.drwil/ia-first.json` → `checks`); the command is never copied here.
`**Manual**` describes the part of the proof that stays human. At least one
of the two, plus a **Rule**; an unknown check is an error. A contract still
in table form (legacy) is still read, but without a Check it is never
proven (`MANUAL`).
`**Severity**` (optional): `blocking` (default), `warning` or `advisory`.
Only blocking contracts decide the `drwil verify` verdict; the others are
run and shown without changing its exit code.
`**Origin**` (optional): the workstream and the acceptance criterion the
contract was born from (`**Origin**: <workstream> — « <criterion> »`). When
its check fails, `drwil verify` shows it: the failure breaks that criterion,
a regression unless the active brief declares the change (« Modified
contracts » section).

## Security (SEC)

## SEC-006 — No known vulnerable dependency
**Rule** : No dependency with a known vulnerability.
**Scope** : project dependencies
**Source of truth** : dependency manifest (e.g. package.json, requirements.txt)
**Proof** : audit declared by the project (`.drwil/ia-first.json` → `checks`), hook + CI
**Reason** : a vulnerable library voids the rest
**Manual** : no dependency audit declared: add one in `.drwil/ia-first.json` → `checks` (with an `id`), then cite it here as **Check**.

## SEC-007 — No secret in the repo
**Rule** : No secret in the repo (`.env`, keys, passwords, client configs).
**Scope** : the whole repo and its history
**Source of truth** : —
**Proof** : gitleaks: indexed files (hook, if installed) + full history (CI job, if configured)
**Reason** : an irreversible leak once pushed
**Check** : `secrets`


## Quality (QUA)

## QUA-011 — Docs never wrong
**Rule** : Every repo path and every contract ID cited in the docs and the skills exist (IDs defined exactly once in the registry).
**Scope** : `AGENTS.md`, layer `AGENTS.md` files, `docs/`, `.claude/skills/` (if present)
**Source of truth** : —
**Proof** : `.githooks/check-docs.mjs` (hook + CI)
**Reason** : wrong docs mislead AIs
**Check** : `docs-references`

## QUA-013 — A check that did not run has not passed
**Rule** : No check can disappear without it being visible. A check that cannot run on a workstation (missing tool) is only ever a warning, never a silent success. A degradable check must also have a CI job triggered on the same paths (verified, not just assumed).
**Scope** : the checks in `.githooks/run-checks.mjs`
**Source of truth** : `.githooks/run-checks.mjs`, `.githooks/check-control-coverage.mjs` and its run history
**Proof** : **machine**: every check ends up "failed", "OK" or "not run", never hidden; CI coverage is replayed (hook + CI, if there is a CI)
**Reason** : a check that can silently not run is, eventually, a check that proves nothing
**Check** : `ci-coverage`

## QUA-015 — Projects resumable cold
**Rule** : Projects resumable cold: every open item in the project index carries an `[AI]`, `[human]` or `[decision]` marker; every brief in `docs/projects/` and `docs/intentions/` (excluding the index and README) has a dated "Status" line at the top (YYYY-MM-DD); every intent brief has "Need", "Current state" and "Open questions" sections; every brief has a "Hand-off" section (handoff). Extension: an index item that cites a brief whose "Status" contains `done`/`closed`/`terminé` must be checked, and vice versa (non-blocking warning).
**Scope** : `docs/projects/`, `docs/intentions/`
**Source of truth** : `docs/ia-first.md` (section 7)
**Proof** : `.githooks/check-docs.mjs` (hook + CI); accuracy of status and content: **human**
**Reason** : an agent picks up a project cold: without status or marker, it redoes the work, attempts a human gesture or decides in the requester's place
**Check** : `docs-references`
**Manual** : accuracy of the briefs' status and content.

## QUA-016 — Scoping reminder
**Rule** : Scoping reminder: every indexed code file is covered by the `scope` block of a brief in `docs/projects/`; a malformed block (no `files:` line, too broad a pattern) is rejected. `docs/` and any markdown file are never code. Severity configurable per project (`.drwil/ia-first.json` -> `scopeReminder`: `blocking` by default, refused at commit and, with Claude Code, before the write; `warning` to only report; `off` to disable). Outside the `blocking` setting, this contract is not proven: add a Manual field to it.
**Scope** : the whole repo (indexed files), excluding `docs/` and markdown
**Source of truth** : `.githooks/scope.mjs`, `docs/ia-first.md` (section 7)
**Proof** : `.githooks/check-docs.mjs` (hook + CI, severity per setting); agent side: `.claude/hooks/scope-reminder.mjs` (if present: refusal before the write when `blocking`, reminder when `warning`, silent when `off`)
**Reason** : code detached from any project gets lost, for an agent as much as for a person
**Check** : `docs-references`

## QUA-017 — No direct work on the main branch
**Rule** : No direct work on the main branch: committing or pushing directly to `master`/`main` is refused, including the very first commit. `init()` always switches to a working branch before any commit, with no configurable opt-out. Work must go through a branch (`docs/recipes/working-with-branches.md`) merged via a pull/merge request.
**Scope** : any git repository, including before its very first commit, local only (`pre-commit`/`pre-push`)
**Source of truth** : `docs/recipes/working-with-branches.md`
**Proof** : `.githooks/run-checks.mjs` (hook only, never in CI: a legitimate merge onto the main branch must not be blocked retroactively)
**Reason** : a direct commit or push to the main branch bypasses review and breaks the brief-branch link
**Check** : `main-branch`

## QUA-019 — No bypassing a check
**Rule** : A blocking check (test, hook, CI) is never disabled, skipped (`--no-verify`, skip, exception flag) or bypassed to make a task progress. If it seems miscalibrated, flag it and fix the check itself (or open a decision brief), never disable it silently.
**Scope** : any check in `.githooks/`, any CI, any test
**Source of truth** : this contract (recalled in `AGENTS.md`)
**Proof** : **human** — a bypass rarely leaves a machine-checkable trace (see `catalog:QUA-009` for the tooled case of a disabled test)
**Reason** : bypassing a check silently hides a regression, the opposite of QUA-013
**Manual** : a bypass rarely leaves a machine-checkable trace.

## QUA-029 — A contract's cited tests exist and carry its ID
**Rule** : Every file cited by the **Tests** field of a contract exists and contains the contract's ID (cited in the name of the test that protects it). Stack-agnostic: test names are not parsed. Blocking if the project enforces recent requirements (`newRequirements`: `blocking`, default of fresh installs), warning otherwise.
**Scope** : the **Tests** field of this registry
**Source of truth** : `.githooks/check-contract-tests.mjs`
**Proof** : `.githooks/check-contract-tests.mjs` (hook + CI, `drwil verify`)
**Reason** : a business rule is only a non-regression while its test still exists: a deleted or renamed test would leave the contract green while proving nothing
**Check** : `contract-tests`

## QUA-040 — The architecture map covers the code
**Rule** : Every folder holding code (the project's code prefixes and layers, two levels deep) is cited in `docs/architecture.md`, a project file `init` never rewrites; the setting `architectureMap.files` requires every file under chosen paths. Blocking if the project enforces recent requirements (`newRequirements`: `blocking`), warning otherwise. A workstream that changes the structure updates the map in its closing commit. The map, README and changelog are written in the `docLang` language (absent: the project language). The check judges that a folder is cited, not that its written role is right: that part stays human.
**Scope** : `docs/architecture.md`
**Source of truth** : `.githooks/check-architecture.mjs`
**Proof** : `.githooks/check-architecture.mjs` (hook + CI, `drwil verify`)
**Reason** : a map no one maintains goes stale silently, and an agent arriving later reads a wrong picture of the project
**Check** : `architecture-map`

## Business (APP)

## APP-001 — Example business contract, to replace with your first one
**Rule** : (example) An order with no line is refused. Replace it with the most critical business rule of your project.
**Scope** : the code that applies this rule
**Source of truth** : the module that implements it
**Proof** : a test of this rule, named after its ID, declared as its own check (`.drwil/ia-first.json` → `checks`, with an `id`, filtering the tests by the ID: `npm test -- -t "APP-001"`), replayed by `drwil verify` at every workstream
**Reason** : an acceptance criterion proven by a contract becomes the business non-regression: the next workstreams cannot break it without seeing it, and its own check names the broken rule
**Manual** : check to complete — write the test, declare it in `checks` with an `id`, cite it here as **Check**, list its file as **Tests**, then remove this field (`docs/recipes/adopt-the-kit.md`, step 5).


## Drum box (DBX)

## DBX-001 — Sixteen steps play in time at the chosen tempo
**Rule** : The sequencer fires the 16 sixteenth-note steps of the pattern at `60 / tempo / 4` seconds each (within one sample), loops after step 16, and its output does not depend on the audio block size.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Sequencer.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-001` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : a drum machine that drifts or depends on the audio buffer is unusable
**Check** : `dbx-001`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box — « The 16-step sequencer plays a pattern in time at the chosen tempo »

## DBX-002 — Swing shifts the off-beat steps, 0% is straight
**Rule** : Swing (0–100%) delays only odd (off-beat) steps by `swing × half a step`; at 0% every step sits exactly on the grid.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Sequencer.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-002` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : swing is the groove of the machine, and 0% must be a neutral baseline
**Check** : `dbx-002`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box — « Swing shifts the off-beat steps and 0% swing leaves the grid straight »

## DBX-003 — A step is off, normal or accent, and an accent is louder
**Rule** : Each step cell is off, normal or accent; an off step is silent and an accented hit is at least 1.3 times the peak of a normal one on every track.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Pattern.h`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-003` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : dynamics per step are what makes a funk pattern
**Check** : `dbx-003`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box — « A step can be off, normal or accented, and an accent is louder »

## DBX-004 — Eight tracks, eight distinct synthesised sounds
**Rule** : The tracks kick, snare, closed hat, open hat, clap, tom, rim and cowbell each produce a finite, bounded sound that ends, and any two differ in waveform, zero-crossing rate or length; a closed hat chokes the open hat.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Voices.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-004` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : the voices must be told apart by ear, with no sample file
**Check** : `dbx-004`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box — « Eight tracks (kick, snare, closed hat, open hat, clap, tom, rim, cowbell) each produce a distinct synthesised sound »

## DBX-005 — The standalone app starts, plays and stops
**Rule** : The standalone JUCE app target builds, and the engine behind its PLAY/STOP button starts from step 1, stops stepping on stop, and applies tempo and swing changes while playing. Opening the window and the audio device is not exercised by a machine (checked by hand under DBX-006).
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/MainComponent.cpp`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-005` (`.drwil/ia-first.json`), hook + `drwil verify`; the build needs the JUCE system dependencies (X11, ALSA, FreeType development packages on Linux)
**Reason** : the first thing a user does is press play and stop
**Check** : `dbx-005`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box — « The standalone app starts, plays and stops from its own window »

## DBX-006 — The interface is easy to use and has a 70's funk feel
**Rule** : The standalone window is easy to use (16×8 step grid, tempo and swing knobs, one play/stop button, readable states) and has a 70's funk look: warm orange, brown and mustard palette, wood grain, chunky knobs.
**Scope** : the standalone window (`drumbox/Source/MainComponent.cpp`, `drumbox/Source/FunkLookAndFeel.cpp`)
**Source of truth** : `drumbox/Source/FunkLookAndFeel.cpp`
**Proof** : **human** — attested by the requester after trying the app; no machine can judge taste or ease of use
**Reason** : the look and feel is the product's identity, a matter of taste that only the requester can accept
**Manual** : the human tries the app and attests that the interface is easy to use and has a 70's funk feel (`drwil attest`, human gesture).
**Origin** : drum-box — « The interface is easy to use and has a 70's funk feel »

## DBX-007 — Patterns chain into a longer sequence that loops
**Rule** : The song holds 16 patterns (each with the drums, the bass line and the synth) and a chain of 1 to 128 positions naming the pattern played at each; the sequencer plays the patterns of the chain one after the other in time (16 steps each, within one sample, independent of the audio block size), then loops to the first position. A chain of one position plays that pattern alone, exactly as `DBX-001` to `DBX-005` require.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Pattern.h`, `drumbox/Source/Sequencer.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-007` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : a song is more than one bar, and chaining must not disturb the timing of a single pattern
**Check** : `dbx-007`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-song — « Up to 16 patterns, each holding the drums, the bass line and the synth, can be chained (up to 128 positions) and play one after the other in time, then loop »

## DBX-008 — The bass is a monophonic voice playing one note per step at the step's pitch
**Rule** : Each step of a pattern can carry one bass note (pitch 24–96, MIDI numbering) and an accent flag; the bass is a single synthesised voice, so a new note takes over the one still sounding; each note plays at the pitch set on its step (within 3%), an empty step is silent, an accented note is at least 1.3 times the peak of a normal one, and the sound is finite, bounded and ends.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Instruments.cpp`, `drumbox/Source/Pattern.h`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-008` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : a bass line is a melody of single notes; two bass notes at once would turn it into mud
**Check** : `dbx-008`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-song — « The bass line is a monophonic synthesised voice playing one note per step, at the pitch set on that step »

## DBX-009 — The synth has four voices, two oscillators, a filter, an envelope and an LFO
**Rule** : Each step of a pattern can carry up to four synth notes (and an accent flag) that play together; the synth has four voices (a fifth note takes over the oldest), each with two oscillators (the second tunable in semitones and cents), a low-pass filter, an attack-decay-sustain-release envelope, and one LFO that sweeps the filter cutoff (zero depth leaves the sound still); an empty step is silent, an accent is at least 1.3 times the peak of a normal step, and the sound is finite, bounded and ends.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Instruments.cpp`, `drumbox/Source/Pattern.h`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-009` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : chords and pads are what a synth adds next to the bass and the drums; its controls must each audibly do their job
**Check** : `dbx-009`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-song — « The synth is a four-voice polyphonic voice (two oscillators, filter, envelope, LFO) playing the notes set on its steps »

## DBX-010 — The bass, synth and chain editors are easy to use and keep the 70's funk feel
**Rule** : The standalone window lets the user edit the bass line and the synth step by step (note on/off, pitch by drag or wheel, accent), choose among the 16 patterns, and build the chain (a position grid and its length), while staying easy to use and keeping the 70's funk look of `DBX-006`.
**Scope** : the standalone window (`drumbox/Source/MainComponent.cpp`, `drumbox/Source/FunkLookAndFeel.cpp`)
**Source of truth** : `drumbox/Source/MainComponent.cpp`
**Proof** : **human** — attested by the requester after trying the app; no machine can judge taste or ease of use
**Reason** : the editors are the product's face, a matter of taste that only the requester can accept
**Manual** : the human tries the app and attests that the bass, synth, pattern bank and chain editors are easy to use and keep the 70's funk feel (`drwil attest`, human gesture).
**Origin** : drum-box-song — « The interface lets the user edit the bass and the synth, build the chain, and stays easy to use with the 70's funk feel »

## DBX-026 — The pluck is a four-voice synth whose notes start at once and decay quickly
**Rule** : Each step of a pattern can carry up to four pluck notes (and an accent flag) that play together; the pluck has four voices (a fifth note takes over the oldest), each with an instant attack, a short decay that ends the note even while the key is held, and a low-pass filter that closes quickly along with the decay; each note plays at the pitch set on its step (within 3%), an empty step is silent, an accent is at least 1.3 times the peak of a normal step, and the sound is finite, bounded and ends. The sound of the drums, the bass and the synth is unchanged by the pluck.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Instruments.cpp`, `drumbox/Source/Pattern.h`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-026` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : plucked chords and stabs are a different voice from the pad: they must hit at once and fade fast, without disturbing the other tracks
**Check** : `dbx-026`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-pluck — « The pluck is a four-voice polyphonic synth whose notes start at once, decay quickly and close the filter quickly, and it plays the notes set on its steps »

## DBX-027 — Each pattern holds pluck notes and the chain plays them in time
**Rule** : Each of the 16 patterns holds its own pluck notes (pitch 24–96, MIDI numbering, up to four per step, plus an accent flag) next to its drums, bass and synth; the sequencer plays them with the pattern chain, one after the other in time (within one sample, independent of the audio block size) and then loops, as it does for the other tracks.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Pattern.h`, `drumbox/Source/Sequencer.cpp`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-027` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : a song needs the pluck line to change from pattern to pattern, in time with the rest
**Check** : `dbx-027`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-pluck — « Each pattern holds the pluck notes next to the drums, the bass and the pad, and the pattern chain plays them in time »

## DBX-028 — The PAD and PLUCK views are easy to use and keep the 70's funk feel
**Rule** : The standalone window shows the existing synth as PAD, offers a PLUCK view to edit the pluck notes step by step (note on/off, pitch by drag or wheel, accent) and its sound knobs, while staying easy to use and keeping the 70's funk look of `DBX-006`.
**Scope** : the standalone window (`drumbox/Source/MainComponent.cpp`, `drumbox/Source/FunkLookAndFeel.cpp`)
**Source of truth** : `drumbox/Source/MainComponent.cpp`
**Proof** : **human** — attested by the requester after trying the app; no machine can judge taste or ease of use
**Reason** : the editors are the product's face, a matter of taste that only the requester can accept
**Manual** : the human tries the app and attests that the PAD view and the PLUCK view are easy to use and keep the 70's funk feel (`drwil attest`, human gesture).
**Origin** : drum-box-pluck — « The interface shows the existing synth as PAD, offers a PLUCK view to edit the pluck notes and sound, and stays easy to use with the 70's funk feel »

## DBX-030 — The FX track has six distinct synthesised sounds, each producing sound when triggered
**Rule** : The FX track holds six sounds, one step row each: siren (pitch sweeping up then down), laser (fast falling pitch), riser (noise rising in loudness and brightness), impact (low thump with a noise burst), whoosh (short white noise) and scratch (a saw dragged back and forth); each produces a finite, bounded sound that ends, any two differ in waveform, zero-crossing rate or length, an off step is silent and an accented hit is at least 1.3 times the peak of a normal one. The sound of the drums, the bass, the pad and the pluck is unchanged by the FX.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Voices.cpp`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-030` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : sirens and other noises are the colour of a funk break: each must be told apart by ear, with no sample file, and must not disturb the other tracks
**Check** : `dbx-030`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-fx — « The FX track has six distinct synthesised sounds — siren, laser, riser, impact, whoosh and scratch — each producing sound when triggered »

## DBX-031 — Each pattern holds FX steps and the chain plays them in time
**Rule** : Each of the 16 patterns holds its own FX steps (six rows of 16 steps, each off, normal or accent, as for the drums) next to its drums, bass, pad and pluck; the sequencer plays them with the pattern chain, one after the other in time (within one sample, independent of the audio block size) and then loops, as it does for the other tracks. The FX steps add no drum trigger.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Pattern.h`, `drumbox/Source/Sequencer.cpp`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-031` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : a song needs its effects to change from pattern to pattern, in time with the rest
**Check** : `dbx-031`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-fx — « Each pattern holds the FX steps next to the other tracks, and the pattern chain plays them in time »

## DBX-032 — The FX view is easy to use and keeps the 70's funk feel
**Rule** : The standalone window offers an FX view with the six step rows (siren, laser, riser, impact, whoosh, scratch; click a step: off, normal, accent), while staying easy to use and keeping the 70's funk look of `DBX-006`.
**Scope** : the standalone window (`drumbox/Source/MainComponent.cpp`, `drumbox/Source/FunkLookAndFeel.cpp`)
**Source of truth** : `drumbox/Source/MainComponent.cpp`
**Proof** : **human** — attested by the requester after trying the app; no machine can judge taste or ease of use
**Reason** : the editors are the product's face, a matter of taste that only the requester can accept
**Manual** : the human tries the app and attests that the FX view is easy to use and keeps the 70's funk feel (`drwil attest`, human gesture).
**Origin** : drum-box-fx — « The interface offers an FX view with the six step rows, and stays easy to use with the 70's funk feel »

## DBX-011 — A muted track is silent, and with solo only the soloed tracks are heard
**Rule** : Each of the twelve tracks (the eight drums, the bass, the pad, the pluck and the FX) can be muted and soloed, song-wide and outside any pattern; a muted track is silent (even when also in solo), with one or several tracks in solo only those tracks are heard, with none in solo every unmuted track is heard; the other tracks are unchanged. With no mute and no solo the sound is as before.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Mixer.h`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-011` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : mixing what you hear is the first use of a track list; a mute must never colour the other tracks
**Check** : `dbx-011`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-mixer — « Each of the twelve tracks can be muted or soloed, several tracks can be in solo together »

## DBX-012 — Each track has its own volume
**Rule** : Each of the twelve tracks has a volume (0 to 150 %, 100 % by default, song-wide and outside any pattern): a lower volume makes that track quieter in proportion and leaves the other tracks unchanged; at 100 % the sound is as before.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Mixer.h`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-012` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : balancing the tracks against each other is what a mixer is for
**Check** : `dbx-012`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-mixer — « Each track has its own volume »

## DBX-029 — Each track has a tune (±24 semitones) and a detune (±100 cents)
**Rule** : Each of the twelve tracks has a tune (−24 to +24 semitones) and a detune (−100 to +100 cents), song-wide and outside any pattern: the pitch of a drum voice or of an FX voice, or of every note of the bass, the pad and the pluck, moves by that amount, and the other tracks are unchanged; at 0 the sound is as before. A noise-only drum (the rim click's noise) keeps its colour.
**Scope** : the drum box engine (`drumbox/Source/`)
**Source of truth** : `drumbox/Source/Mixer.h`, `drumbox/Source/Voices.cpp`, `drumbox/Source/Instruments.cpp`, `drumbox/Source/DrumEngine.cpp`
**Proof** : test named after its ID in `drumbox/tests/EngineTests.cpp`, run by the check `dbx-029` (`.drwil/ia-first.json`), hook + `drwil verify`
**Reason** : a drum kit or a line is tuned to the song without editing its notes
**Check** : `dbx-029`
**Tests** : `drumbox/tests/EngineTests.cpp`
**Origin** : drum-box-mixer — « Every track gets a tune (±24 semitones) and a detune (±100 cents) »

## DBX-013 — The TRACKS view stacks the twelve tracks and is easy to use with the 70's funk feel
**Rule** : The standalone window offers a TRACKS view that stacks the eight drums, the bass, the pad, the pluck and the FX one above the other, each with the patterns of the chain where it plays, mute, solo, volume, tune and detune, and where clicking a track opens its editor, while staying easy to use and keeping the 70's funk look of `DBX-006`.
**Scope** : the standalone window (`drumbox/Source/MainComponent.cpp`, `drumbox/Source/FunkLookAndFeel.cpp`)
**Source of truth** : `drumbox/Source/MainComponent.cpp`
**Proof** : **human** — attested by the requester after trying the app; no machine can judge taste or ease of use
**Reason** : the track view is the product's face, a matter of taste that only the requester can accept
**Manual** : the human tries the app and attests that the TRACKS view shows the twelve tracks stacked with mute, solo, volume, tune and detune, that clicking a track opens its editor, and that it is easy to use and keeps the 70's funk feel (`drwil attest`, human gesture).
**Origin** : drum-box-mixer — « A TRACKS view stacks the twelve tracks one above the other, with mute, solo, volume, tune and detune on each, and clicking a track opens its editor »

## Out of registry (these are not contracts)

| Rule | Nature | Where it lives |
|---|---|---|
| Commits, personal data in an AI's outputs, secrets in tool output, attack testing on a real target, docs kept current in the same commit | conduct (human) | `AGENTS.md`, Conduct section |
| Refactor without behavior change | procedure (human) | `docs/recipes/refactor-without-breaking.md` |
| File size, project-specific code hygiene rules | opt-in tooling (no universal threshold) | `.githooks/check-file-size.mjs`, `.githooks/check-code-rules.mjs`, `docs/recipes/refactor-without-breaking.md` |
| Tests shipped with every new component | convention (human) | the relevant layer's `AGENTS.md` |
