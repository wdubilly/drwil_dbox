# Project: Drum box, track view, mute, solo and track volume

**Status**: scoped on 2026-10-21 — not started.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/track-view-mixer.md`

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

Let the user mix what they hear and see the whole song at a glance: mute or
solo any of the ten tracks (the eight drums, the bass, the synth), set a
volume per track, and see every track stacked one above the other in a track
view. Windows and Linux only.

## Acceptance criteria

- A muted track is silent; with one or several tracks in solo, only those tracks are heard; with none in solo, all unmuted tracks are heard — `DBX-011`
- Each track has its own volume: a lower volume makes that track quieter and leaves the others unchanged — `DBX-012`
- A TRACKS view stacks the eight drums, the bass and the synth one above the other, with mute, solo and volume on each, and clicking a track opens its editor [human] — `DBX-013`

## 2. Out of scope

Pan, editing notes inside the track view, applying mute and solo to the WAV
export (decided with the export brief), saving the mixer state, effects,
presets, piano roll, macOS.

## 3. Constraints

- Builds on the existing engine: mixing logic stays free of JUCE and testable without an audio device.
- Existing contracts `DBX-001` to `DBX-010` keep passing unchanged: with no mute, no solo and default volumes the sound is as today.
- Mute, solo and volume are song-wide mixer settings, not stored in a pattern.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).
- The Linux app build needs the X11, ALSA and FreeType development packages.

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: the track view is read-only; clicking a track opens its editor, approved by the requester.
- 2026-10-21: the track view is a new TRACKS view next to the current ones, approved by the requester.
- 2026-10-21: volume per track, pan later, approved by the requester.
- 2026-10-21: several tracks can be in solo together, approved by the requester.
- 2026-10-21: the agent proposes the TRACKS layout in the existing 70's funk style, judged by the requester under `DBX-013`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — mixer in the engine** [AI]: mute, solo and volume per track applied when mixing. Exit criterion: `DBX-011` and `DBX-012` tests pass and `DBX-001` to `DBX-010` still pass.
- **Increment 2 — TRACKS view** [AI]: ten stacked tracks with the pattern chain, mute, solo, volume and a click to open the track's editor, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-013` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): scoped, not started. Engine in `drumbox/Source/` (`DrumEngine.*` mixes the voices), UI in `MainComponent.*` with views DRUMS, BASS, SYNTH and CHAIN.
- **Uncommitted work**: none once committed.
- **Next step**: [human] start the workstream (`/drwil-start`); then the agent runs increment 1.
