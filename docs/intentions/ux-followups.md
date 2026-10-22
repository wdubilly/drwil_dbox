# Intent: Drum box interface, follow-ups of the redesign

**Status**: proposed on 2026-10-22, written when the workstream `drum-box-ux` was closed.
**Priority**: P2

## Need

Limits left untreated by the interface redesign, to decide one by one:

- No warning when the song has unsaved changes and the user opens another one, loads the demo or quits.
- The interface is not tested by a machine: only the engine pieces behind it (undo, settings, demo song) are; the layout, the skins and the workflow rest on a human attestation (`DBX-036`, `DBX-037`). A screenshot-based check could be considered.
- Undo steps are recorded every tenth of a second at most, once the mouse is up: two edits made within that time form one step.
- The chain strip scrolls sideways beyond about twenty positions; there is no overview of a long chain.
- Windows and Linux only; macOS stays out.

## Current state

Read on 2026-10-22 in `drumbox/Source/MainComponent.cpp` (history commit in the timer, no dirty flag), `drumbox/Source/Widgets.cpp` (`ChainStrip`) and `docs/contracts.md` (`DBX-036`, `DBX-037` are human proofs).

## Open questions

- [decision] Is an unsaved-changes warning wanted, and on which actions (open, demo, quit)?
- [decision] Is an automated interface check (screenshots on a virtual display in CI) worth its cost?
- [decision] Is a finer undo granularity needed, or is one step per gesture enough?
- [decision] Is an overview of long chains wanted?
