# Project: Drum box, pattern editing and piano roll

**Status**: scoped on 2026-10-21 — not started.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/pattern-editing.md`

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

Make building songs more comfortable: copy, paste, clear and swap patterns,
a chain whose length can change while playing without surprises, and a piano
roll for the bass, the pad and the pluck next to the step-by-step entry. Windows and
Linux only.

## Acceptance criteria

- A pattern can be copied and pasted onto another, cleared, or swapped with another, and the other patterns are untouched — `DBX-022`
- When the chain length changes during playback, the new length applies at once: playback goes on if the current position is still inside the chain, otherwise it returns to the first position — `DBX-023`
- Notes entered in the piano roll give the same pattern data as the step-by-step entry, and the step-by-step entry still works — `DBX-024`
- The pattern operations and the piano roll are easy to use in the interface, with the 70's funk feel [human] — `DBX-025`

## 2. Out of scope

Note lengths beyond one step, velocity editing curves, undo and redo, MIDI
import, saving (done with the songs brief), macOS.

## 3. Constraints

- Starts after the songs workstream is closed: the piano roll edits the same pattern data that the song file saves, so the `.dbox` format does not change.
- Builds on the engine: pattern operations and note entry logic stay free of JUCE and testable without an audio device.
- Existing contracts `DBX-001` to `DBX-021` keep passing unchanged.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: operations are copy, paste, clear and swap, approved by the requester.
- 2026-10-21: a chain length change applies at once, approved by the requester.
- 2026-10-21: piano roll for the bass, the pad and the pluck, the step-by-step entry kept, approved by the requester.
- 2026-10-21: the workstream comes last in the queue, after the songs workstream, approved by the requester.
- 2026-10-21: when the position falls outside the new chain length, playback returns to the first position: the agent's proposal, judged under `DBX-023`.
- 2026-10-21: the agent proposes the piano roll layout in the existing 70's funk style, judged by the requester under `DBX-025`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — pattern operations** [AI]: copy, paste, clear, swap. Exit criterion: `DBX-022` tests pass.
- **Increment 2 — chain length change** [AI]: apply a new length at once during playback. Exit criterion: `DBX-023` tests pass.
- **Increment 3 — piano roll** [AI]: note entry grid for the bass, the pad and the pluck writing the pattern data. Exit criterion: `DBX-024` tests pass.
- **Increment 4 — interface** [AI]: pattern operation buttons and piano roll view, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-025` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): scoped, not started. The songs workstream is closed (see the journal).
- **Uncommitted work**: none once committed.
- **Next step**: [human] start the workstream (`/drwil-start`); then the agent runs increment 1.
