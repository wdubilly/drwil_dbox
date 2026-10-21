#pragma once

#include "DrumEngine.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace drumbox
{
constexpr int exportSampleRate = 44100;
constexpr int exportBitDepth = 24;
// How long the effect tails are followed after the chain, at most.
constexpr int maxTailSeconds = 60;

// Rendering is offline: it works on a copy of the song, never touches the engine it is given (nor
// its sound device), and gives the same samples whatever the machine is doing. The chain is played
// once from its first position, then the tails of the effects and of the voices are followed until
// silence. The engine is mono, so the stereo files carry the same signal on both channels.
// Mute and solo are applied: a track that is not heard is silent in the mix and in its stem.
// A stem is the track alone through the master chain, with its volume, tune and send, so the stems
// do not add up exactly to the mix when a compressor, a distortion or a reverb is on.

// Samples of the chain alone, and of the chain with its tails (the length of every exported file).
struct ExportLength
{
    size_t chainFrames = 0;
    size_t totalFrames = 0;
};
ExportLength measureExport (DrumEngine& engine);

struct ExportedAudio
{
    ExportLength length;
    std::vector<float> mix;
    std::array<std::vector<float>, numMixerTracks> stems;
};
// The mix alone, in memory.
std::vector<float> renderSongMix (DrumEngine& engine);
// Mix and stems in memory, for short songs and for the tests; the files below stream instead.
ExportedAudio renderSongAudio (DrumEngine& engine);

// Streams a stereo 24-bit WAV file whose length in frames is known beforehand.
class WavFileWriter
{
public:
    // The data goes to `<path>.part`, renamed to `path` by finish() so a failure leaves no half file.
    bool open (const std::string& path, size_t frames);
    void write (const float* mono, int numSamples);
    bool finish();
    ~WavFileWriter();

private:
    void discard();

    std::ofstream file;
    std::string target;
    size_t expected = 0;
    size_t written = 0;
};

bool exportMixWav (DrumEngine& engine, const std::string& path);
// Writes `<base>_mix.wav` and `<base>_<NN>_<TRACK>.wav` for the twelve tracks in the folder, all of the same
// length. False when the name is not a safe file name or a file cannot be written.
bool exportStemsWav (DrumEngine& engine, const std::string& folder, const std::string& baseName);
} // namespace drumbox
