# Closed projects journal

Condensed summary of each `docs/projects/` brief once its Status moves to
"done" (the whole brief, not just one point among several of a brief that
otherwise stays open). The detailed brief is then deleted: no more full
history in plain text in the repository, but nothing is lost —
`git log --follow -- docs/projects/<brief>.md` (or the closing commit/PR
cited below) retrieves the full text at any time.

Goal: keep `docs/projects/` light (active briefs only) without sacrificing
traceability, which stays in git anyway. See the recipe
`docs/recipes/working-with-branches.md`, "Closing" section.

One entry per closed project, most recent last.

## Drum box, first slice (standalone, 8 synthesised tracks) — closed 2026-10-21

Delivered a JUCE/C++ standalone drum machine with a 70's funk look: 16-step sequencer with tempo, swing and accents (`DBX-001` to `DBX-003`), eight synthesised voices (`DBX-004`), standalone app that builds and whose transport logic is tested (`DBX-005`), look and ease of use attested by the human (`DBX-006`). GitHub CI runs the repository checks on Linux and builds the Windows `.exe` as an artifact. Engine kept free of JUCE so it is tested without an audio device. Left out: plugin formats, samples, pattern saving, export, separate outputs, macOS CI (see `docs/intentions/drum-box.md`). Limits: the Linux app build needs X11, ALSA and FreeType development packages; the example contract `APP-001` is still to replace. Full text: the git history of the drum-box brief (`git log --follow`).

## Drum box, bass line, synth and pattern chain — closed 2026-10-21

Extended the standalone drum box: a bank of 16 patterns (drums, bass and synth together) chained over up to 128 positions and looped (`DBX-007`), a monophonic synthesised bass with one pitch per step (`DBX-008`), a four-voice polyphonic synth with two oscillators, filter, envelope and LFO (`DBX-009`), and DRUMS, BASS, SYNTH and CHAIN views in the funk look, attested by the human (`DBX-010`). `DBX-001` to `DBX-005` kept passing unchanged. Windows and Linux only; the Windows `.exe` comes from the CI artifact. Limits, now in `docs/intentions/pattern-editing.md`: no pattern copy in the interface, a jump when the chain length changes during playback, step-by-step entry only (piano roll later). Full text: the git history of the brief (`git log --follow`).

## Drum box, pluck synth track — closed 2026-10-21

Added a pluck synth track: four polyphonic voices with an instant attack, a short decay and a fast-closing filter (`DBX-026`), its notes stored in each pattern and played by the chain (`DBX-027`), and a PLUCK view in the interface, the existing synth being renamed PAD, attested by the human (`DBX-028`). The sound of the drums, the bass and the pad is unchanged. Windows and Linux only. Full text: the git history of the brief (`git log --follow`).

## Drum box, sound effects track — closed 2026-10-22

Added an FX track of six synthesised sounds: siren, laser, riser, impact, whoosh and scratch (`DBX-030`), six FX step rows stored in each pattern and played by the chain (`DBX-031`), and an FX view in the interface, attested by the human (`DBX-032`). The sound of the other tracks is unchanged. Windows and Linux only. Limit: no demo FX hits in the starting song. Full text: the git history of the brief (`git log --follow`).

## Drum box, track view, mute, solo, volume and tuning — closed 2026-10-22

Added a song-wide mixer over the 12 tracks: mute and solo, with mute taking priority (`DBX-011`), volume from 0 to 150 % (`DBX-012`), tune of ±24 semitones and detune of ±100 cents (`DBX-029`), and a TRACKS view stacking every track with its chain map, attested by the human (`DBX-013`). Gains glide over about 3 ms to avoid clicks. Windows and Linux only. Limits: the rim noise component does not follow the tune; mute and solo are not yet applied to the WAV export (next brief). Full text: the git history of the brief (`git log --follow`).
