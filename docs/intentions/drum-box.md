# Intent: Drum box rétro (boîte à rythmes 70's funk)

**Status**: proposed on 2026-10-21.

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

Read on 2026-10-21: the repository holds only the governance kit; no
application code, stack "not decided" (`docs/architecture.md` not re-read).

## Open questions

- [decision] Stack: JUCE C++ confirmed? (proposed, not approved)
- [decision] First slice proposed: standalone, 8 synthesised tracks, 16 steps,
  tempo, swing, accents. Accepted, or another cut?
- [decision] Later slices order: samples, pattern saving, MIDI/audio export,
  separate outputs, plugin formats.
- [decision] Licence of the project (JUCE is GPL/commercial) and source of
  royalty-free samples.
- [decision] AU is only buildable on macOS: CI for the three platforms?
- [decision] "70's funk" visual direction: references or mock-ups to follow?
