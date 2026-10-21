#pragma once

#include "DrumEngine.h"

#include <string>

namespace drumbox
{
constexpr int songFormatVersion = 1;
constexpr const char* songFileExtension = ".dbox";

// The whole song as JSON: tempo, swing, the 16 patterns and the chain, the sound knobs of the bass, pad
// and pluck, every track's mute, solo, volume, tune, detune and send, and the settings of the five effects.
// Every number is written so that reading it back gives the same value.
std::string serializeSong (DrumEngine& engine);

// Replaces the song of the engine with the one in the text. False when the text is not a song of this
// format or is damaged, and then the engine is left exactly as it was; out-of-range settings are brought
// back into range.
bool parseSong (const std::string& text, DrumEngine& engine);

// Files: the path must end with `.dbox`. A save never leaves a half-written file behind.
bool saveSongFile (DrumEngine& engine, const std::string& path);
bool loadSongFile (DrumEngine& engine, const std::string& path);
} // namespace drumbox
