#pragma once

#include "Pattern.h"
#include "Sequencer.h"
#include "Voices.h"

#include <atomic>
#include <vector>

namespace drumbox
{
constexpr float normalGain = 0.6f;
constexpr float accentGain = 1.0f;

// Everything the audio callback needs, with no dependency on JUCE or on a device.
// Setters may be called from the UI thread; render() runs on the audio thread.
class DrumEngine
{
public:
    DrumEngine();

    void prepare (double sampleRate);

    Pattern& getPattern() noexcept { return pattern; }

    void setTempo (double bpm) noexcept { tempoRequest.store (bpm); }
    void setSwing (double percent) noexcept { swingRequest.store (percent); }
    void setPlaying (bool shouldPlay) noexcept { playRequest.store (shouldPlay); }
    bool isPlaying() const noexcept { return playRequest.load(); }

    // Index of the step last triggered, or -1 when stopped.
    int getCurrentStep() const noexcept { return currentStep.load(); }

    // Mono output, mixed in `out`.
    void render (float* out, int numSamples);

private:
    Pattern pattern;
    Sequencer sequencer;
    VoiceBank voices;
    std::vector<Trigger> triggers;
    std::atomic<double> tempoRequest { 120.0 };
    std::atomic<double> swingRequest { 0.0 };
    std::atomic<bool> playRequest { false };
    std::atomic<int> currentStep { -1 };
    bool wasPlaying = false;
};
} // namespace drumbox
