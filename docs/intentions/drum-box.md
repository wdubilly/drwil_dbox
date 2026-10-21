# Intent: Drum box rétro (boîte à rythmes 70's funk)

**Status**: first slice delivered on 2026-10-21; later slices still to scope.
**Reviewed on 2026-10-21**: stack (JUCE, C++), licence (GPL), first slice and Windows/Linux CI are decided and delivered; plugin formats, samples, pattern saving, export and separate outputs remain.

## Need

"Faire un drum box, type boîte à rythmes à l'ancienne. Un plugin que je puisse
exploiter en standalone." Wanted: as complete as possible, user friendly,
70's funk look and feel.

- Formats: VST3, AU, CLAP and standalone; JUCE (C++) proposed.
- Platforms: Linux, Windows, macOS.
- Sounds: synthesis (808/909 style) and loadable samples.
- Sequencer: 16 steps, tempo, swing, accent/velocity per step, 8 to 12 tracks.
- Also listed: pattern saving, audio/MIDI export, separate outputs per track.

## Current state

Read on 2026-10-21: a JUCE standalone app (16 steps, tempo, swing, accents, 8 synthesised tracks) built with CMake, with engine tests and GitHub CI (Linux checks, Windows `.exe` artifact). See `docs/architecture.md`.

## Open questions

- [decision] Order of the later slices: samples, pattern saving, MIDI/audio export, separate outputs, plugin formats.
- [decision] Source of royalty-free samples.
- [decision] AU is only buildable on macOS: add macOS CI?
- [decision] The example contract `APP-001` is still in `docs/contracts.md`: replace it with a real rule or remove it.
