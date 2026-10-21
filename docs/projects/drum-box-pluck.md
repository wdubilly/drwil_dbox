# Project: Drum box, pluck synth track

**Status**: scoped on 2026-10-21 — not started.
**Risk**: HIGH
**Maturity**: ready
**Intent**: none (requested in conversation, framed directly)

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

Add a second synth track for plucked sounds. The existing synth keeps its
sound and becomes the pad (the « nappe » sound): the song then has eleven
tracks (eight drums, bass, pad, pluck). Windows and Linux only.

## Acceptance criteria

- The pluck is a four-voice polyphonic synth whose notes start at once, decay quickly and close the filter quickly, and it plays the notes set on its steps — `DBX-026`
- Each pattern holds the pluck notes next to the drums, the bass and the pad, and the pattern chain plays them in time — `DBX-027`
- The interface shows the existing synth as PAD, offers a PLUCK view to edit the pluck notes and sound, and stays easy to use with the 70's funk feel [human] — `DBX-028`

## 2. Out of scope

Changing the sound of the existing synth (it only gets the name PAD in the
interface), mixer, effects, presets, saving, piano roll, macOS, further
synth tracks.

## 3. Constraints

- Builds on the existing engine: the pluck voice stays free of JUCE and testable without an audio device.
- Existing contracts `DBX-001` to `DBX-010` keep passing unchanged: the sound of the drums, the bass and the pad is as today.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).
- The Linux app build needs the X11, ALSA and FreeType development packages.

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: the existing synth becomes the pad and keeps its sound; only a pluck track is added, approved by the requester.
- 2026-10-21: pluck is four voices with an instant attack, a short decay and a fast-closing filter, approved by the requester.
- 2026-10-21: this workstream comes first in the queue, before the mixer, effects, songs and editing workstreams, approved by the requester.
- 2026-10-21: the agent proposes the PLUCK view in the existing 70's funk style, judged by the requester under `DBX-028`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — pluck voice** [AI]: four-voice pluck synth. Exit criterion: `DBX-026` tests pass and `DBX-001` to `DBX-010` still pass.
- **Increment 2 — pattern data** [AI]: pluck notes stored in each pattern and played by the chain. Exit criterion: `DBX-027` tests pass.
- **Increment 3 — interface** [AI]: SYNTH renamed PAD, PLUCK view with its notes and knobs, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-028` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): scoped, not started. First of the queue: pluck, FX, then mixer, effects, songs, editing.
- **Uncommitted work**: none once committed.
- **Next step**: [human] start the workstream (`/drwil-start`); then the agent runs increment 1.
