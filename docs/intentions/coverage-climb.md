# Intent: engine coverage, climb from the ratchet to 100 %

**Status**: proposed on 2026-10-27, written when the coverage guarantee of the engine was closed.
**Priority**: P2

## Need

The engine's coverage is held by a ratchet at the measured level (lines 93 %, branches 60 %, functions 99 %), with 100 % required on new or modified code only. The old code stays below 100 %; raise the thresholds of `coverage-root` in `.drwil/ia-first.json` as tests are added, up to 100 %.

## Current state

Measured on 2026-10-27 with gcovr over `drumbox/Source` without the JUCE interface: the least covered files are `SongExport.cpp` (86 % of lines), `SongFile.cpp` (88 %) and `Sequencer.cpp` (89 %); branches (60 %) include many compiler-generated paths. `npx drwil guarantees` does not know C++ and reports « stack not detected ».

## Open questions

- [decision] Should branch coverage count generated exception branches (`--exclude-throw-branches`) to make 100 % reachable?
- [decision] Which files first: the least covered, or the ones changed most?
