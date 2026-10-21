# Intent: Sound presets and effects

**Status**: scoped on 2026-10-21 into `docs/projects/drum-box-effects.md`; the intent is removed at the workstream's closing.

## Need

"Avoir des presets de son de synthé et de basse line, ce serait cool, et quelques effets."

- Presets for the synth and the bass sound, selectable in the interface.
- A few effects.

## Current state

Read on 2026-10-21 (`docs/architecture.md`): standalone JUCE app, 16-pattern bank with a chain of up to 128 positions, eight drum voices, a monophonic bass and a four-voice synth, views DRUMS, BASS, SYNTH and CHAIN. Windows and Linux. No saving, no export, no per-track mixing, no presets, no effects.

## Open questions

- [decision] Which effects (reverb, delay, chorus, distortion, filter sweep, compressor)?
- [decision] Effects per track, on the bass and synth only, or on a master bus?
- [decision] Presets: a factory set only, or can the user save their own?
- [decision] Do presets cover drum kits too?
- [decision] Does the preset or effect setting belong to the song file?
