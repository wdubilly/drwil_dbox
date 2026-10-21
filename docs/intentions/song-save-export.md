# Intent: Save songs and export to WAV

**Status**: proposed on 2026-10-21.

## Need

"Enregistrer nos morceaux et les exporter en wave aussi."

- Save a song (patterns, chain, tempo, swing, bass and synth settings) and load it back.
- Export the song to a WAV file.

## Current state

Read on 2026-10-21 (`docs/architecture.md`): standalone JUCE app, 16-pattern bank with a chain of up to 128 positions, eight drum voices, a monophonic bass and a four-voice synth, views DRUMS, BASS, SYNTH and CHAIN. Windows and Linux. No saving, no export, no per-track mixing, no presets, no effects.

## Open questions

- [decision] Song file format: own file (e.g. JSON) or something else; one file per song?
- [decision] WAV export: the whole chain once or looped N times; sample rate and bit depth (16/24-bit)?
- [decision] Export as one stereo mix, or one file per track (stems)?
- [decision] Where do songs live (a chosen folder, a default songs folder)? Autosave?
- [decision] Include a reverb or delay tail at the end of the export?
