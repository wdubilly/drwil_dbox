# Project: Drum box, interface redesign (workflow, skins, undo)

**Status**: implemented on 2026-10-22 — engine tests green (`DBX-033` to `DBX-035`), app builds and links on Linux; `DBX-036` and `DBX-037` (and the rewritten interface contracts) await the human attestation.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/ux-followups.md`

<!-- scope
files:
  - drumbox/CMakeLists.txt
  - drumbox/Source/*
  - drumbox/tests/*
  - .drwil/ia-first.json
  - docs/contracts.md
  - docs/architecture.md
-->

## 1. Need

Make the app easy to adopt: one Sequence view with the twelve tracks stacked (mute, solo and volume on each row), a detail panel for the selected track (its sound, piano roll and effects) replacing the per-instrument tabs, a fixed transport bar, presets first with a demo song at first launch, tooltips, undo and redo, a playhead on the grid and the pattern chain as a strip under the grid. A skin selector: the default skin is dark and flat with one accent colour per track, the 70's funk look stays as an option. The window is resizable, scales with the screen and remembers its size. Windows and Linux only.

## Acceptance criteria

- Every edit of the song (steps, notes, pattern operations, chain, mixer, effects, tempo, swing, presets) can be undone and redone, and undo then redo returns exactly the same song; a new edit after an undo drops the redo history — `DBX-033`
- The chosen skin, the window size and the first-launch flag are saved and restored between runs; a missing or damaged settings file gives the defaults without failing — `DBX-034`
- At first launch a demo song is loaded; it plays audible sound on drums, bass and pad, and it is a valid song file — `DBX-035`
- The Sequence view, the detail panel, the fixed transport bar, the pattern chain strip, the presets-first flow, the tooltips and the playhead make the main workflow easy to adopt: starting from the demo, a new user can change a pattern, a sound and the tempo without looking for a tab [human] — `DBX-036`
- The default skin is dark and flat with one accent colour per track, the selector switches to the 70's funk skin, and the layout scales with the screen without overlap or clipping [human] — `DBX-037`

## Modified contracts

- `DBX-006` — former criterion: « The interface is easy to use and has a 70's funk feel » — reason: the default look becomes dark and flat; the funk look stays as a skin option, judged under `DBX-037` (approved on 2026-10-22)
- `DBX-010` — former criterion: « The bass, synth and chain editors are easy to use and keep the 70's funk feel » — reason: the per-instrument tabs are replaced by the Sequence view, the detail panel and the chain strip, judged under `DBX-036` (approved on 2026-10-22)
- `DBX-013` — former criterion: « The TRACKS view stacks the twelve tracks and is easy to use with the 70's funk feel » — reason: the TRACKS tab becomes the Sequence view, judged under `DBX-036` (approved on 2026-10-22)
- `DBX-017` — former criterion: « The effects and presets are easy to use and sound good in the 70's funk spirit » — reason: effects and presets move to the detail panel and the presets-first flow, judged under `DBX-036` (approved on 2026-10-22)
- `DBX-021` — former criterion: « Saving, opening and exporting are easy to use » — reason: the buttons move to the fixed transport bar, judged under `DBX-036` (approved on 2026-10-22)
- `DBX-025` — former criterion: « The pattern operations and the piano roll are easy to use and keep the 70's funk feel » — reason: the piano roll moves to the detail panel and the pattern operations next to the chain strip, judged under `DBX-036` (approved on 2026-10-22)
- `DBX-028` — former criterion: « The PAD and PLUCK views are easy to use and keep the 70's funk feel » — reason: the PAD and PLUCK tabs are replaced by the detail panel, judged under `DBX-036` (approved on 2026-10-22)
- `DBX-032` — former criterion: « The FX view is easy to use and keeps the 70's funk feel » — reason: the FX tab is replaced by the detail panel, judged under `DBX-036` (approved on 2026-10-22)

## 2. Out of scope

New sounds, new effects, new tracks, MIDI import or export, samples, plugin formats, a skin editor or more than two skins, macOS, any change to the engine sound (`DBX-001` to `DBX-032` keep passing unchanged), a change of the `.dbox` file format other than what undo needs (none expected).

## 3. Constraints

- The engine stays free of JUCE and testable without an audio device: undo and redo, the settings and the demo song are plain C++ with tests in `drumbox/tests/EngineTests.cpp`.
- Undo and redo covers the whole song, engine included, as the requester decided on 2026-10-22.
- The layout follows the screen: resizable window, remembered size; the minimum size (960 by 600 pixels) is a technical choice, adjustable without a new decision.
- The look is centralised so that a skin is a swap of colours and shapes, not a second layout.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).

## 4. Decisions

- 2026-10-22: the old tabs are replaced, no parallel old layout, approved by the requester.
- 2026-10-22: everything in one workstream, approved by the requester.
- 2026-10-22: skin selector; the default skin is dark and flat with one accent colour per track; the 70's funk look stays as an option, approved by the requester.
- 2026-10-22: the window size follows the screen, approved by the requester.
- 2026-10-22: undo and redo covers the whole song, engine included, approved by the requester.
- 2026-10-22: platforms for now are Windows and Linux only.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — undo and redo** [AI]: an edit history over the whole song in the engine. Exit criterion: `DBX-033` tests pass.
- **Increment 2 — settings and demo song** [AI]: settings file (skin, window size, first launch) and a demo song. Exit criterion: `DBX-034` and `DBX-035` tests pass.
- **Increment 3 — skins and scaling** [AI]: dark flat skin with a colour per track, funk skin kept, selector, resizable layout. Exit criterion: the app builds on Linux and in the Windows CI job.
- **Increment 4 — new layout** [AI]: Sequence view, detail panel, transport bar, chain strip, tooltips, playhead, presets-first flow, undo and redo controls. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-036` and `DBX-037` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-22): all four increments implemented on branch `workstream/drum-box-ux`; `node .githooks/run-checks.mjs` green; the interface was built and linked on Linux but never run on a screen by the agent.
- **Uncommitted work**: committed with the closing.
- **Next step**: [human] try the app and attest `DBX-036`, `DBX-037` and the rewritten `DBX-006`, `DBX-010`, `DBX-013`, `DBX-017`, `DBX-021`, `DBX-025`, `DBX-028`, `DBX-032` (`drwil attest`); follow-ups in `docs/intentions/ux-followups.md`.
