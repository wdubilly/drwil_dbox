# Intent: Track view, mute and solo

**Status**: proposed on 2026-10-21.

## Need

"Avoir la possibilité de mute ou jouer en solo une piste; j'aimerai une vue piste aussi, genre chaque élément basse, batterie l'un au dessus de l'autre."

- Mute and solo on each track (the eight drums, the bass, the synth).
- A track view: every element stacked one above the other (drum tracks, bass, synth), on a time line.

## Current state

Read on 2026-10-21 (`docs/architecture.md`): standalone JUCE app, 16-pattern bank with a chain of up to 128 positions, eight drum voices, a monophonic bass and a four-voice synth, views DRUMS, BASS, SYNTH and CHAIN. Windows and Linux. No saving, no export, no per-track mixing, no presets, no effects.

## Open questions

- [decision] Does the track view only show the pattern chain per instrument, or also let the user edit the notes in place?
- [decision] Per-track volume and pan alongside mute and solo?
- [decision] Does the track view replace the current DRUMS, BASS, SYNTH and CHAIN views or sit next to them?
- [decision] Does mute and solo apply to the WAV export?
