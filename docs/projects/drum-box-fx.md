# Project: Drum box, sound effects track

**Status**: increments 1 to 3 built on 2026-10-21 (`DBX-030`, `DBX-031` checks pass, FX view built); `DBX-032` waits for the human attestation.
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

Add a track of sound effects (sirens and other noises) triggered on the steps
like the drums. It is one track of the song, with six synthesised sounds, one
step row each. The song then has twelve tracks (eight drums, bass, pad,
pluck, FX). Windows and Linux only.

## Acceptance criteria

- The FX track has six distinct synthesised sounds — siren (up and down sweep), laser, riser (rising noise), impact, whoosh (short white noise) and scratch — each producing sound when triggered — `DBX-030`
- Each pattern holds the FX steps (off, normal or accented, as for the drums) next to the other tracks, and the pattern chain plays them in time — `DBX-031`
- The interface offers an FX view with the six step rows, and stays easy to use with the 70's funk feel [human] — `DBX-032`

## 2. Out of scope

Loading samples, a pitch per step on the FX sounds, mixer, effects, presets,
saving, piano roll, macOS, further FX sounds.

## 3. Constraints

- Builds on the existing engine: the FX voices stay free of JUCE and testable without an audio device.
- Existing contracts `DBX-001` to `DBX-010` and `DBX-026` to `DBX-028` keep passing unchanged: the other tracks sound as before.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).
- The Linux app build needs the X11, ALSA and FreeType development packages.

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: the FX track is a trigger track like the drums, with several synthesised sounds, one per row, no pitch per step, approved by the requester.
- 2026-10-21: the six sounds are siren, laser, riser, impact, whoosh and scratch, approved by the requester.
- 2026-10-21: this workstream comes second in the queue, right after the pluck and before the mixer, approved by the requester.
- 2026-10-21: the FX counts as one track for the mixer, effects and presets (one volume, mute, solo, tune and send for the six rows together): the agent's proposal, to be corrected by the requester if each row should be its own track.
- 2026-10-21: the agent proposes the FX view in the existing 70's funk style, judged by the requester under `DBX-032`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — FX voices** [AI]: six synthesised sounds. Exit criterion: `DBX-030` tests pass and `DBX-001` to `DBX-010` still pass.
- **Increment 2 — pattern data** [AI]: FX steps stored in each pattern and played by the chain. Exit criterion: `DBX-031` tests pass.
- **Increment 3 — interface** [AI]: FX view with six step rows, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-032` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): FX engine (six voices, FX rows in each pattern, played by the chain) and FX view done on branch `workstream/drum-box-fx`; `DBX-030` and `DBX-031` tests pass, `DBX-001` to `DBX-010` and `DBX-026` to `DBX-028` still pass; the Linux app builds. Not closed.
- **Uncommitted work**: none once committed.
- **Next step**: [human] try the FX view in the app (Windows `.exe` from the CI run) and attest `DBX-032` (`drwil attest`); then the agent closes the workstream (journal, `node .githooks/close.mjs`).
