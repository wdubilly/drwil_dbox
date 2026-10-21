# Project: Drum box, track view, mute, solo, volume and tuning

**Status**: 2026-10-22 — increment 1 done (mixer in the engine, `DBX-011`, `DBX-012`, `DBX-029` tested); increment 2 built (TRACKS view, Linux app builds), CI Windows build green, `DBX-013` attested by the human on 2026-10-22; ready to close.
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
solo any of the twelve tracks (the eight drums, the bass, the pad, the pluck, the FX), set a
volume, a tune and a detune per track, and see every track stacked one above the other in a track
view. Windows and Linux only.

## Acceptance criteria

- A muted track is silent; with one or several tracks in solo, only those tracks are heard; with none in solo, all unmuted tracks are heard — `DBX-011`
- Each track has its own volume: a lower volume makes that track quieter and leaves the others unchanged — `DBX-012`
- Each track has a tune (±24 semitones) and a detune (±100 cents): the pitch of a drum voice, or of every note of the bass, the pad and the pluck, moves by that amount, and the other tracks are unchanged; at 0 the sound is as today — `DBX-029`
- A TRACKS view stacks the eight drums, the bass, the pad, the pluck and the FX one above the other, with mute, solo, volume, tune and detune on each, and clicking a track opens its editor [human] — `DBX-013`

## 2. Out of scope

Pan, editing notes inside the track view, applying mute and solo to the WAV
export (decided with the export brief), saving the mixer state, effects,
presets, piano roll, macOS.

## 3. Constraints

- Builds on the existing engine: mixing logic stays free of JUCE and testable without an audio device.
- Existing contracts `DBX-001` to `DBX-010` keep passing unchanged: with no mute, no solo and default volumes the sound is as today.
- Mute, solo, volume, tune and detune are song-wide mixer settings, not stored in a pattern.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).
- The Linux app build needs the X11, ALSA and FreeType development packages.

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: the track view is read-only; clicking a track opens its editor, approved by the requester.
- 2026-10-21: the track view is a new TRACKS view next to the current ones, approved by the requester.
- 2026-10-21: volume per track, pan later, approved by the requester.
- 2026-10-21: every track gets a tune (±24 semitones) and a detune (±100 cents), added to this workstream next to the volume, approved by the requester.
- 2026-10-21: several tracks can be in solo together, approved by the requester.
- 2026-10-21: the agent proposes the TRACKS layout in the existing 70's funk style, judged by the requester under `DBX-013`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — mixer in the engine** [AI]: mute, solo, volume, tune and detune per track applied when mixing. Exit criterion: `DBX-011`, `DBX-012` and `DBX-029` tests pass and `DBX-001` to `DBX-010` still pass.
- **Increment 2 — TRACKS view** [AI]: twelve stacked tracks with the pattern chain, mute, solo, volume, tune, detune and a click to open the track's editor, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-013` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-22): engine mixer in `drumbox/Source/Mixer.h` and `DrumEngine.cpp` (per-track mute, solo, volume, tune, detune; gains glide in ~3 ms); `DBX-001` to `DBX-005`, `DBX-007` to `DBX-009`, `DBX-011`, `DBX-012`, `DBX-026`, `DBX-027`, `DBX-029` to `DBX-031` pass locally. TRACKS view (`TrackStrip` in `MainComponent.*`, twelve stacked strips, click a track or its chain map to open its editor); the Linux app builds. Known limit: the rim's noise component does not follow the tune (its two tones do).
- **Uncommitted work**: none once committed.
- **Next step**: [agent] close the workstream (journal entry, `node .githooks/close.mjs`); `DBX-013` attested.
