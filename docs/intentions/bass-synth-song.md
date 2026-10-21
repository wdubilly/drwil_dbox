# Intent: bass line, synth and longer sequences

**Status**: scoped on 2026-10-21 into `docs/projects/drum-box-song.md`; the intent is removed at the workstream's closing.

## Need

"Ajouter une basse line et un synth et aussi pouvoir avoir plus de longueur soit en mettant des patterns sur un plus long séquenceur." Platforms for now: Windows and Linux only (no macOS).

- A bass line and a synth, next to the eight drum tracks.
- Longer music than one 16-step pattern: patterns chained on a longer sequencer.

## Current state

Read on 2026-10-21 (`docs/architecture.md`): one 16-step pattern, eight synthesised drum voices, tempo and swing, JUCE standalone app. No pitched voice, no note data, no pattern chaining.

## Open questions

- [decision] Bass and synth played step by step (pitch per step) or with a keyboard / piano roll?
- [decision] Monophonic or polyphonic (bass, synth)?
- [decision] Synth sound: one oscillator and filter, or several; which parameters are exposed?
- [decision] Longer sequence: a chain of patterns (how many patterns, maximum chain length) or a longer step grid (32, 64 steps)?
- [decision] Do the drums, bass and synth share the same patterns and chain?
- [decision] Delivery order: longer sequences first, or bass and synth first?
