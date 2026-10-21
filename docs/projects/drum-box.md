# Project: Drum box, first slice (standalone, 8 synthesised tracks)

**Status**: scoped on 2026-10-21 — not started.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/drum-box.md`

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

A retro drum machine with a 70's funk look and feel, usable standalone. This
first slice delivers the playable core; plugin formats, samples and export
come later (see the intent).

## Acceptance criteria

- The 16-step sequencer plays a pattern in time at the chosen tempo — `DBX-001`
- Swing shifts the off-beat steps and 0% swing leaves the grid straight — `DBX-002`
- A step can be off, normal or accented, and an accent is louder — `DBX-003`
- Eight tracks (kick, snare, closed hat, open hat, clap, tom, rim, cowbell) each produce a distinct synthesised sound — `DBX-004`
- The standalone app starts, plays and stops from its own window — `DBX-005`
- The interface is easy to use and has a 70's funk feel [human] — `DBX-006`

## 2. Out of scope

Plugin formats (VST3, AU, CLAP), sample loading, pattern saving, audio and
MIDI export, separate outputs, tracks beyond eight, CI for three platforms.

## 3. Constraints

- JUCE, C++, built with CMake; project under GPL.
- Linux first for the first slice; code kept portable to Windows and macOS.
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).
- Sequencing and synthesis logic testable without an audio device.

## 4. Decisions

- 2026-10-21: stack JUCE (C++), approved by the requester.
- 2026-10-21: licence GPL, chosen by the requester.
- 2026-10-21: the agent proposes the 70's funk palette and layout, judged by the requester under `DBX-006`.
- 2026-10-21: first slice as listed above, approved by the requester.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — project skeleton** [AI]: CMake and JUCE standalone app opening an empty window; lint and test commands declared in `.drwil/ia-first.json`. Exit criterion: build and tests pass.
- **Increment 2 — sequencer and synthesis** [AI]: step engine with tempo, swing, accents; eight synthesised voices. Exit criterion: `DBX-001` to `DBX-004` tests pass.
- **Increment 3 — interface** [AI]: 16-step grid, transport, tempo and swing controls, funk styling. Exit criterion: `DBX-005` passes, `DBX-006` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): brief scoped, no code.
- **Uncommitted work**: this brief, the intent and its index line.
- **Next step**: [human] start with `/drwil-start`.
