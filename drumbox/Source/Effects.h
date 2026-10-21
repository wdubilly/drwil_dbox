#pragma once

#include <array>
#include <atomic>
#include <vector>

namespace drumbox
{
// Every setting is a float so presets and the UI treat them alike; `on` is 0 or 1.
// The UI writes, the audio thread reads.
struct ReverbParams
{
    std::atomic<float> on { 0.0f };
    std::atomic<float> size { 0.6f };  // 0..1, length of the tail
    std::atomic<float> damp { 0.5f };  // 0..1, how fast the highs die
    std::atomic<float> mix { 0.3f };   // 0..1, level of the tail added to the sound
};

struct DelayParams
{
    std::atomic<float> on { 0.0f };
    std::atomic<float> timeMs { 375.0f }; // 20..1000
    std::atomic<float> feedback { 0.4f }; // 0..0.9
    std::atomic<float> mix { 0.3f };      // 0..1, level of the echoes added to the sound
};

struct DistortionParams
{
    std::atomic<float> on { 0.0f };
    std::atomic<float> drive { 0.5f }; // 0..1
    std::atomic<float> tone { 0.6f };  // 0..1, low-pass after the clipper
    std::atomic<float> mix { 1.0f };   // 0..1, dry to distorted
};

struct ChorusParams
{
    std::atomic<float> on { 0.0f };
    std::atomic<float> rate { 0.8f };  // Hz, 0.1..5
    std::atomic<float> depth { 0.5f }; // 0..1
    std::atomic<float> mix { 0.5f };   // 0..1, dry to chorused
};

struct CompressorParams
{
    std::atomic<float> on { 0.0f };
    std::atomic<float> threshold { -18.0f }; // dB, -48..0
    std::atomic<float> ratio { 4.0f };       // 1..20
    std::atomic<float> makeup { 0.0f };      // dB, 0..18
};

// One setting of an effect, for the UI and for the tests.
struct EffectSetting
{
    const char* name;
    std::atomic<float>* value;
    float min, max;
};

class Reverb
{
public:
    void prepare (double sampleRate);
    float process (float in, const ReverbParams& params) noexcept;

private:
    static constexpr int numCombs = 4;
    static constexpr int numAllpasses = 2;
    std::array<std::vector<float>, numCombs> combs;
    std::array<size_t, numCombs> combPos {};
    std::array<float, numCombs> combLow {};
    std::array<std::vector<float>, numAllpasses> allpasses;
    std::array<size_t, numAllpasses> allpassPos {};
};

class Delay
{
public:
    void prepare (double sampleRate);
    float process (float in, const DelayParams& params) noexcept;

private:
    double sampleRate = 44100.0;
    std::vector<float> buffer;
    size_t pos = 0;
};

class Distortion
{
public:
    void prepare (double sampleRate) noexcept { this->sampleRate = sampleRate; low = 0.0f; }
    float process (float in, const DistortionParams& params) noexcept;

private:
    double sampleRate = 44100.0;
    float low = 0.0f;
};

class Chorus
{
public:
    void prepare (double sampleRate);
    float process (float in, const ChorusParams& params) noexcept;

private:
    double sampleRate = 44100.0;
    std::vector<float> buffer;
    size_t pos = 0;
    double phase = 0.0;
};

class Compressor
{
public:
    void prepare (double sampleRate) noexcept { this->sampleRate = sampleRate; envelope = 0.0f; }
    float process (float in, const CompressorParams& params) noexcept;

private:
    double sampleRate = 44100.0;
    float envelope = 0.0f;
};

// The master chain: distortion, chorus, compressor, delay, reverb, in that order. An effect that is
// off is skipped as if it were not there; with all five off the sound goes through untouched.
class EffectsRack
{
public:
    EffectsRack() { prepare (44100.0); }

    void prepare (double sampleRate);

    ReverbParams& reverb() noexcept { return reverbParams; }
    DelayParams& delay() noexcept { return delayParams; }
    DistortionParams& distortion() noexcept { return distortionParams; }
    ChorusParams& chorus() noexcept { return chorusParams; }
    CompressorParams& compressor() noexcept { return compressorParams; }

    // Settings of one effect (0 = reverb, 1 = delay, 2 = distortion, 3 = chorus, 4 = compressor); the first one is `on`.
    std::vector<EffectSetting> settings (int effect);
    static const char* effectName (int effect) noexcept;

    bool anyOn() const noexcept;
    float process (float in) noexcept;

private:
    ReverbParams reverbParams;
    DelayParams delayParams;
    DistortionParams distortionParams;
    ChorusParams chorusParams;
    CompressorParams compressorParams;
    Reverb reverbFx;
    Delay delayFx;
    Distortion distortionFx;
    Chorus chorusFx;
    Compressor compressorFx;
};

constexpr int numEffects = 5;
} // namespace drumbox
