# Architecture — drwil_dbox

As of 2026-10-21. Map of the repository: what lives where. Keep it up to date in the same commit as the code it describes.

## What the application does

Drum box: a retro drum machine with a 70's funk look, usable standalone. 16-step sequencer (tempo, swing, off/normal/accent steps), eight synthesised tracks (kick, snare, closed hat, open hat, clap, tom, rim, cowbell). First slice: standalone only; plugin formats, samples, pattern saving and export come later (`docs/intentions/drum-box.md`).

## Stack

C++17 with JUCE 8 (GPL, fetched by CMake `FetchContent`), built with CMake and Ninja. Linux first, code kept portable to Windows and macOS. Decided by the requester on 2026-10-21. On Linux the standalone app needs the X11, ALSA and FreeType development packages; the engine and its tests need only a C++ compiler and CMake.

## Layers

Two parts inside `drumbox/`, kept apart so the logic is testable with no audio device:

- Engine (pure C++, no JUCE): `drumbox/Source/Pattern.h` — the 16×8 step grid (off/normal/accent); `drumbox/Source/Sequencer.h` and `drumbox/Source/Sequencer.cpp` — clock, tempo, swing; `drumbox/Source/Voices.h` and `drumbox/Source/Voices.cpp` — the eight synthesised voices; `drumbox/Source/DrumEngine.h` and `drumbox/Source/DrumEngine.cpp` — what the audio callback runs. Built as the static library `drumbox_engine`.
- Standalone app (JUCE): `drumbox/Source/Main.cpp` — application and window; `drumbox/Source/MainComponent.h` and `drumbox/Source/MainComponent.cpp` — step grid, transport, tempo and swing knobs, audio device; `drumbox/Source/FunkLookAndFeel.h` and `drumbox/Source/FunkLookAndFeel.cpp` — the 70's funk look.
- `drumbox/CMakeLists.txt` — build; `drumbox/tests/EngineTests.cpp` — plain C++ tests, run through CTest (a test name starts with its contract ID, `DBX-001` to `DBX-005`).
- `.github/workflows/` — GitHub CI: `.github/workflows/ia-first.yml` runs the repository checks on Linux and builds the Windows standalone app, uploading the `.exe` as an artifact.

Build and test: `cmake -S drumbox -B drumbox/build/engine -G Ninja -DDRUMBOX_BUILD_APP=OFF && cmake --build drumbox/build/engine && ctest --test-dir drumbox/build/engine` (engine only); without `-DDRUMBOX_BUILD_APP=OFF` the JUCE app is built too. The same commands are declared as checks in `.drwil/ia-first.json`.

## AI-first governance

- AI instructions: `AGENTS.md`, then the `AGENTS.md` of the layer you touch.
- Invariants: `docs/contracts.md`.
- Recipes: `docs/recipes/`.
- Checks: `.githooks/run-checks.mjs`, run on commit by `.githooks/pre-commit`. Stack-specific checks are declared in `.drwil/ia-first.json` (`checks` key).
- Kit configuration: `.drwil/ia-first.json`.
