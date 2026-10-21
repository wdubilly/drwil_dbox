# Project: Drum box, save songs and export to WAV

**Status**: scoped on 2026-10-21 — not started.
**Risk**: HIGH
**Maturity**: ready
**Intent**: `docs/intentions/song-save-export.md`

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

Keep the music: save a whole song in a file and open it again, and export it
to WAV, as a stereo mix and as one file per track. Windows and Linux only.

## Acceptance criteria

- A song saved to a `.dbox` file and opened again is identical: patterns, chain, tempo, swing, bass, synth, mixer, effects and presets — `DBX-018`
- The WAV export of the song is a stereo 24-bit 44.1 kHz file holding the chain played once with the effect tails, with mute and solo applied — `DBX-019`
- The stems export writes one WAV file per track, plus the stereo mix, all of the same length — `DBX-020`
- Saving, opening and exporting are available in the interface, easy to use [human] — `DBX-021`

## 2. Out of scope

MIDI export, separate outputs, other audio formats (MP3, FLAC), autosave,
cloud sync, import of other file formats, macOS.

## 3. Constraints

- Starts after the effects workstream is closed, because the song file holds the mixer, the effects and the presets.
- The song is a JSON file with the extension `.dbox`, one song per file; songs live in a default songs folder, the user picks the file when opening or saving.
- The song and the export logic stay free of JUCE where possible and are testable without an audio device (offline rendering, no real-time).
- Existing contracts `DBX-001` to `DBX-017` keep passing unchanged.
- Linux and Windows builds stay green in CI (`.github/workflows/ia-first.yml`, not modified here).
- Declared lint and test commands must actually run: `QUA-013` (a check that did not run is not a passed check).

## 4. Decisions

- 2026-10-21: platforms for now are Windows and Linux only, approved by the requester.
- 2026-10-21: own JSON file `.dbox`, one song per file, everything included, approved by the requester.
- 2026-10-21: default songs folder; file chosen at opening and saving; no autosave, approved by the requester.
- 2026-10-21: the export is the chain played once with the effect tails, 24-bit, 44.1 kHz, approved by the requester.
- 2026-10-21: the export gives a stereo mix plus one file per track (stems), approved by the requester.
- 2026-10-21: mute and solo apply to the export (answered with the mix choice: « mute et solo appliqués »), approved by the requester.
- 2026-10-21: the agent proposes the file menu and dialogs, judged by the requester under `DBX-021`.

## 5. Points to decide

None open.

## 6. Increments

- **Increment 1 — song file** [AI]: serialise and read back the whole song. Exit criterion: `DBX-018` tests pass.
- **Increment 2 — WAV export** [AI]: offline rendering of the chain once, stereo mix, then stems. Exit criterion: `DBX-019` and `DBX-020` tests pass.
- **Increment 3 — interface** [AI]: save, open and export commands and dialogs, in the funk style. Exit criterion: the app builds on Linux and in the Windows CI job; `DBX-021` attested by the human.

## 7. Hand-off

- **Last state** (2026-10-21): scoped, not started. Starts after the effects workstream (`docs/projects/drum-box-effects.md`) is closed.
- **Uncommitted work**: none once committed.
- **Next step**: [human] start the workstream (`/drwil-start`); then the agent runs increment 1.
