# Project: Drum box, bass line, synth and pattern chain

**Status**: 2026-10-21 — increments 1 to 4 built (chain, bass, synth, interface); `DBX-007` to `DBX-009` tests pass; `DBX-010` waits for the human's attestation.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/bass-synth-song.md`

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

Go beyond one 16-step drum pattern: add a bass line and a synth next to the
eight drum tracks, and let the music run longer by chaining patterns on a
longer sequencer. Windows and Linux only.

## Acceptance criteria

- Up to 16 patterns, each holding the drums, the bass line and the synth, can be chained (up to 128 positions) and play one after the other in time, then loop — `DBX-007`
- The bass line is a monophonic synthesised voice playing one note per step, at the pitch set on that step — `DBX-008`
- The synth is a four-voice polyphonic voice (two oscillators, filter, envelope, LFO) playing the notes set on its steps — `DBX-009`
- The interface lets the user edit the bass and the synth, build the chain, and stays easy to use with the 70's funk feel [human] — `DBX-010`

## 2. Out of scope

Piano roll (a later slice), macOS, plugin formats (VST3, AU, CLAP), sample
loading, pattern or song saving, audio and MIDI export, separate outputs,
more than eight drum tracks, a second pitched instrument beyond one bass and
one synth.

## 3. Constraints

- Builds on the existing engine: sequencing and synthesis logic stays free of JUCE and testable without an audio device.
- Existing contracts `DBX-001` to `DBX-005` keep passing unchanged: one pattern played alone behaves as today.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).
- The Linux app build needs the X11, ALSA and FreeType development packages.

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: notes are entered step by step (one pitch per step); piano roll comes later, approved by the requester (answer « les deux plus tard » read as step-by-step first, piano roll later).
- 2026-10-21: bass monophonic; synth polyphonic with four voices, approved by the requester.
- 2026-10-21: synth sound has two oscillators, a filter, an envelope and an LFO, approved by the requester.
- 2026-10-21: longer sequence is a chain of up to 16 patterns and up to 128 positions, approved by the requester.
- 2026-10-21: a pattern holds the drums, the bass and the synth together, approved by the requester.
- 2026-10-21: one workstream in increments (chain, then bass, then synth), approved by the requester.
- 2026-10-21: the agent proposes the bass and synth screens in the existing 70's funk style, judged by the requester under `DBX-010`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — pattern chain** [AI]: pattern bank (16 patterns, drums only for now), chain of up to 128 positions, looping playback in the sequencer. Exit criterion: `DBX-007` tests pass and `DBX-001` to `DBX-005` still pass.
- **Increment 2 — bass line** [AI]: pitch per step, monophonic bass voice, stored in each pattern. Exit criterion: `DBX-008` tests pass.
- **Increment 3 — synth** [AI]: four-voice polyphonic synth (two oscillators, filter, envelope, LFO), stored in each pattern. Exit criterion: `DBX-009` tests pass.
- **Increment 4 — interface** [AI]: bass and synth step editors, pattern bank and chain editor, funk styling. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-010` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): all four increments built. UI (`MainComponent.*`): views DRUMS, BASS, SYNTH and CHAIN, pattern bank (16 buttons), bass and synth knobs, starter song (patterns 1 and 2, chain 1-1-2-1). The window could not be viewed on the build machine (no display): look and ergonomics are for the human.
- **Uncommitted work**: none once committed.
- **Next step**: [human] try the app (`drumbox/build/app/DrumBox_artefacts/Release/Drum Box`) and run `drwil attest` for `DBX-010`; then close the workstream.
