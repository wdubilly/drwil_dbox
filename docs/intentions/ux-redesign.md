# Intent: UX redesign of the drum box interface

**Status**: stated on 2026-10-22; not scoped yet.
**Priority**: P1

## Need

"En tant qu'expert d'ergonomie appliquée au logiciel de création musicale, remodeler l'interface et l'appli pour une expérience utilisateur optimale. Le workflow actuel me semble peu accessible à adopter."

Proposal agreed with the requester on 2026-10-22, all in one workstream:

1. One **Sequence view**: the 12 tracks stacked, with mute, solo and volume on each row, as the centre of the app.
2. A **detail panel** at the bottom: a click on a track shows its sound, its piano roll and its effects; the per-instrument tabs go away.
3. A **fixed transport bar**: play, tempo, swing, song open/save, export, always visible.
4. **Presets first**: pick a kit or a sound in one click, then tune; a demo song at first launch.
5. **First-step help**: tooltips, undo and redo, a playhead following the playback on the grid.
6. The **pattern chain as a strip** under the grid, with drag and drop.

## Current state

Read on 2026-10-22: eight tabs (DRUMS, BASS, PAD, PLUCK, FX, CHAIN, TRACKS, EFFECTS) in `drumbox/Source/MainComponent.cpp`; the 12 tracks are spread over several views; the mixer is in its own tab; sound settings are bare knobs. The engine (`drumbox/Source/*` without JUCE) and its tests are independent of the layout.

## Open questions

- [decision] Old tabs: decided on 2026-10-22, they are replaced (no parallel old layout).
- [decision] Scope: decided on 2026-10-22, everything in one workstream.
- [decision] Undo and redo: engine-level (every edit) or interface-level only? Not decided.
- [decision] The attested interface contracts (`DBX-006`, `DBX-010`, `DBX-013`, `DBX-017`, `DBX-021`, `DBX-025`, `DBX-028`, `DBX-032`) describe the old tabs: rewrite them for the new layout and have the human attest again.
- [decision] Window size to design for (minimum width and height), and whether the layout must scale.
