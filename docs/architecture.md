# Architecture — drwil_dbox

As of 2026-10-22. Map of the repository: what lives where. Keep it up to date in the same commit as the code it describes.

## What the application does

Drum box: a retro drum machine with a 70's funk look, usable standalone. 16-step sequencer (tempo, swing, off/normal/accent steps), eight synthesised tracks (kick, snare, closed hat, open hat, clap, tom, rim, cowbell), plus a monophonic bass line, a four-voice synth called PAD (two oscillators, filter, envelope, LFO) and a four-voice PLUCK (instant attack, quick decay, fast-closing filter) set note by note on the steps, and an FX track of six synthesised sounds (siren, laser, riser, impact, whoosh, scratch) triggered on step rows like the drums. 16 patterns (each with drums, bass, pad, pluck and FX) are chained over up to 128 positions that play in order, then loop. A song-wide mixer gives each of the twelve tracks mute, solo, volume, tune (±24 semitones) and detune (±100 cents). Standalone only, Windows and Linux; a song is saved to and opened from a `.dbox` JSON file and exported to WAV (stereo mix, or the mix plus one file per track); plugin formats, samples and piano roll come later (`docs/intentions/drum-box.md`).

## Stack

C++17 with JUCE 8 (GPL, fetched by CMake `FetchContent`), built with CMake and Ninja. Linux first, code kept portable to Windows and macOS. Decided by the requester on 2026-10-21. On Linux the standalone app needs the X11, ALSA and FreeType development packages; the engine and its tests need only a C++ compiler and CMake.

## Layers

Two parts inside `drumbox/`, kept apart so the logic is testable with no audio device:

- Engine (pure C++, no JUCE): `drumbox/Source/Pattern.h` — `Pattern` (the 16×8 drum grid off/normal/accent, one bass note per step, up to four pad notes and four pluck notes per step, and six FX step rows) and `Song` (bank of 16 patterns, chain of 1 to 128 positions); `drumbox/Source/Sequencer.h` and `drumbox/Source/Sequencer.cpp` — clock, tempo, swing, walks the chain and emits drum triggers and bass, pad, pluck and FX events; `drumbox/Source/Voices.h` and `drumbox/Source/Voices.cpp` — the eight synthesised drum voices and the six FX voices; `drumbox/Source/Instruments.h` and `drumbox/Source/Instruments.cpp` — the monophonic bass voice, the four-voice synth (PAD) and the four-voice pluck; `drumbox/Source/Mixer.h` — the song-wide mixer settings (mute, solo, volume, tune, detune, effect send for the twelve tracks); `drumbox/Source/Effects.h` and `drumbox/Source/Effects.cpp` — the five effects (reverb, delay, distortion, chorus, compressor) and the master chain; `drumbox/Source/Presets.h` and `drumbox/Source/Presets.cpp` — factory and user presets (drum kits, bass, pad, pluck, FX), user presets stored as files through a small store interface; `drumbox/Source/DrumEngine.h` and `drumbox/Source/DrumEngine.cpp` — what the audio callback runs, applying the mixer; `drumbox/Source/SongFile.h` and `drumbox/Source/SongFile.cpp` — the `.dbox` song file (JSON: tempo, swing, patterns, chain, mixer with mute and solo, sound knobs, effect settings), read whole and checked before it replaces the song; `drumbox/Source/SongExport.h` and `drumbox/Source/SongExport.cpp` — offline WAV export (24-bit, 44.1 kHz, stereo) of the chain played once with its tails, mix and stems, rendered on a copy of the song. Built as the static library `drumbox_engine`.
- Standalone app (JUCE): `drumbox/Source/Main.cpp` — application and window; `drumbox/Source/MainComponent.h` and `drumbox/Source/MainComponent.cpp` — seven views (drums, bass, pad, pluck, FX, chain, tracks; the pad is the existing synth; TRACKS stacks the twelve tracks with their mixer controls), pattern bank, transport, tempo and swing knobs, bass, pad and pluck knobs, the effects view, preset bars, the song buttons (open, save, export WAV, export stems), audio device; `drumbox/Source/FunkLookAndFeel.h` and `drumbox/Source/FunkLookAndFeel.cpp` — the 70's funk look.
- `drumbox/CMakeLists.txt` — build; `drumbox/tests/EngineTests.cpp` — plain C++ tests, run through CTest (a test name starts with its contract ID, `DBX-001` to `DBX-005`, `DBX-007` to `DBX-009`, `DBX-011`, `DBX-012`, `DBX-014` to `DBX-016`, `DBX-018` to `DBX-020`, `DBX-026`, `DBX-027`, `DBX-029`, `DBX-030`, `DBX-031`).
- `.github/workflows/` — GitHub CI: `.github/workflows/ia-first.yml` runs the repository checks on Linux and builds the Windows standalone app, uploading the `.exe` as an artifact.

Build and test: `cmake -S drumbox -B drumbox/build/engine -G Ninja -DDRUMBOX_BUILD_APP=OFF && cmake --build drumbox/build/engine && ctest --test-dir drumbox/build/engine` (engine only); without `-DDRUMBOX_BUILD_APP=OFF` the JUCE app is built too. The same commands are declared as checks in `.drwil/ia-first.json`.

## AI-first governance

- AI instructions: `AGENTS.md`, then the `AGENTS.md` of the layer you touch.
- Invariants: `docs/contracts.md`.
- Recipes: `docs/recipes/`.
- Checks: `.githooks/run-checks.mjs`, run on commit by `.githooks/pre-commit`. Stack-specific checks are declared in `.drwil/ia-first.json` (`checks` key).
- Kit configuration: `.drwil/ia-first.json`.
