#pragma once

#include "Instruments.h"
#include "Pattern.h"
#include "Sequencer.h"
#include "Voices.h"

#include <atomic>
#include <vector>

namespace drumbox
{
constexpr float normalGain = 0.6f;
constexpr float accentGain = 1.0f;
constexpr float bassLevel = 0.5f;
constexpr float synthLevel = 0.5f;
constexpr float pluckLevel = 0.5f;
constexpr float fxLevel = 0.6f;

// Everything the audio callback needs, with no dependency on JUCE or on a device.
// Setters may be called from the UI thread; render() runs on the audio thread.
class DrumEngine
{
public:
    DrumEngine();

    void prepare (double sampleRate);

    Song& getSong() noexcept { return song; }
    // The first pattern of the bank, the one a fresh song plays alone.
    Pattern& getPattern() noexcept { return song.pattern (0); }

    BassParams& getBassParams() noexcept { return bassParams; }
    SynthParams& getSynthParams() noexcept { return synthParams; }
    PluckParams& getPluckParams() noexcept { return pluckParams; }

    void setTempo (double bpm) noexcept { tempoRequest.store (bpm); }
    void setSwing (double percent) noexcept { swingRequest.store (percent); }
    void setPlaying (bool shouldPlay) noexcept { playRequest.store (shouldPlay); }
    bool isPlaying() const noexcept { return playRequest.load(); }

    // Index of the step last triggered, or -1 when stopped.
    int getCurrentStep() const noexcept { return currentStep.load(); }
    // Position in the chain being played, or -1 when stopped.
    int getCurrentChainPosition() const noexcept { return currentChainPosition.load(); }

    // Mono output, mixed in `out`.
    void render (float* out, int numSamples);

private:
    Song song;
    Sequencer sequencer;
    VoiceBank voices;
    FxBank fx;
    BassVoice bass;
    PolySynth synth;
    PluckSynth pluck;
    BassParams bassParams;
    SynthParams synthParams;
    PluckParams pluckParams;
    std::vector<Trigger> triggers;
    std::vector<NoteEvent> notes;
    std::atomic<double> tempoRequest { 120.0 };
    std::atomic<double> swingRequest { 0.0 };
    std::atomic<bool> playRequest { false };
    std::atomic<int> currentStep { -1 };
    std::atomic<int> currentChainPosition { -1 };
    bool wasPlaying = false;
};
} // namespace drumbox
