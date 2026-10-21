# Project: Drum box, effects and sound presets

**Status**: in progress on 2026-10-22 — increments 1 and 2 done (`DBX-014` to `DBX-016` tests pass), increment 3 built (Linux app builds, EFFECTS view, sends in TRACKS, preset selectors); CI green, `DBX-017` attested by the human on 2026-10-22; ready to close.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/sound-presets-effects.md`

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

Shape the sound: five effects (reverb, delay, distortion, chorus, compressor)
on a master chain with a send from each track, and presets for every sound
(drum kits, bass, pad, pluck, FX), factory and user. Windows and Linux only.

## Acceptance criteria

- Each of the five effects (reverb, delay, distortion, chorus, compressor) audibly changes the sound when on and leaves it untouched when off — `DBX-014`
- Each of the twelve tracks has an effect send, and the master chain processes the sum of the sends and the dry mix — `DBX-015`
- Factory presets exist for the drum kit, the bass, the pad, the pluck and the FX; a user can save a preset, load it back and get the same sound — `DBX-016`
- The effects and presets are easy to use in the interface and sound good in the 70's funk spirit [human] — `DBX-017`

## 2. Out of scope

Saving presets and effect settings inside a song file (done with the song
saving brief), pan, WAV export, piano roll, more effects than the five
listed, plugin hosting, macOS.

## 3. Constraints

- Builds on the engine: effects and presets logic stays free of JUCE and testable without an audio device (user preset files are read and written through a small interface the tests replace).
- Existing contracts `DBX-001` to `DBX-013` keep passing unchanged: with every effect off and the default preset the sound is as today.
- User presets are files in a per-user presets folder; factory presets ship with the app.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: effects are reverb, delay, distortion, chorus and compressor, approved by the requester.
- 2026-10-21: one effect chain on the master output plus a send per track, approved by the requester.
- 2026-10-21: presets are factory and user, on every track (drum kits, bass, pad, pluck, FX), approved by the requester.
- 2026-10-21: the agent proposes the effects and presets screens in the existing 70's funk style, judged by the requester under `DBX-017`.
- 2026-10-21: the order of the chain is the agent's proposal, to be judged under `DBX-017`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — effects** [AI]: five effects, master chain, send per track. Exit criterion: `DBX-014` and `DBX-015` tests pass and `DBX-001` to `DBX-013` still pass.
- **Increment 2 — presets** [AI]: factory presets, user presets saved and loaded from files, for drum kits, bass, pad, pluck and FX. Exit criterion: `DBX-016` tests pass.
- **Increment 3 — interface** [AI]: effects panel, send knobs, preset selectors and save button, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-017` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-22): engine (five effects, master chain, send per track, factory and user presets) and interface (EFFECTS view, SEND slider on each track strip, preset selector and SAVE button under each track editor) are implemented on the workstream branch; `DBX-014` to `DBX-016` pass with `DBX-001` to `DBX-013` and `DBX-026` to `DBX-031` still green; the Linux app builds and starts. User presets go to the per-user application data folder, `DrumBox/presets/<kind>/<name>.preset`.
- **Uncommitted work**: none once committed.
- **Next step**: [agent] close the workstream (journal entry, `node .githooks/close.mjs`); `DBX-017` attested.
