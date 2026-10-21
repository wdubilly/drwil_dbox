# Intent: pattern editing comfort and piano roll

**Status**: proposed on 2026-10-21.

## Need

Leftovers of the bass line, synth and pattern chain workstream: editing patterns and notes should be more comfortable.

- Copy a pattern onto another one in the interface.
- Entering notes with a piano roll, in addition to the step-by-step entry (decided as a later slice).
- When the chain length changes during playback, playback can jump to another pattern: make it predictable.

## Current state

Read on 2026-10-21 (`docs/architecture.md`): 16 patterns, a chain of up to 128 positions, notes entered step by step; the engine can change pattern but the interface has no copy.

## Open questions

- [decision] Copy only, or also clear and swap patterns?
- [decision] Piano roll for the bass and the synth: before or after saving songs?
- [decision] On a chain length change while playing: finish the current pattern first, or apply at once?
