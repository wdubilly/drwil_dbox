#pragma once

#include "Pattern.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace drumbox
{
double noteToFrequency (int note) noexcept;

// Linear attack, decay, sustain and release, driven by a gate counted in samples.
class Envelope
{
public:
    struct Settings
    {
        float attack, decay, sustain, release; // seconds, seconds, level, seconds
    };

    void noteOn (int gateSamples) noexcept;
    void noteOff() noexcept;
    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isHeld() const noexcept { return stage != Stage::Idle && stage != Stage::Release; }
    float next (const Settings& settings, double sampleRate) noexcept;

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    float level = 0.0f;
    float releaseStep = 0.0f;
    int gateLeft = 0;
};

// Monophonic: a new note takes over the voice, the pitch is the one of the last note played.
struct BassParams
{
    std::atomic<float> cutoff { 700.0f }; // Hz, opening up to a few times that on each note
    std::atomic<float> decay { 0.25f };   // seconds
};

class BassVoice
{
public:
    void prepare (double newSampleRate) noexcept;
    void noteOn (int note, float gain, int gateSamples) noexcept;
    bool isActive() const noexcept { return envelope.isActive(); }
    void setPitch (double ratio) noexcept { pitch = ratio; }
    float next (const BassParams& params) noexcept;

private:
    double sampleRate = 44100.0;
    Envelope envelope;
    double frequency = 55.0;
    double pitch = 1.0; // frequency ratio from the track tune
    double phase = 0.0;
    float gain = 1.0f;
    float low = 0.0f, band = 0.0f;
};

struct SynthParams
{
    std::atomic<float> osc2Semitones { 0.0f }; // second oscillator, relative to the note
    std::atomic<float> osc2Detune { 9.0f };    // cents
    std::atomic<float> osc2Mix { 0.6f };       // 0 = oscillator 1 only, 1 = oscillator 2 only
    std::atomic<float> cutoff { 2200.0f };     // Hz
    std::atomic<float> resonance { 0.2f };     // 0..1
    std::atomic<float> attack { 0.015f };      // seconds
    std::atomic<float> decay { 0.3f };         // seconds
    std::atomic<float> sustain { 0.65f };      // level
    std::atomic<float> release { 0.25f };      // seconds
    std::atomic<float> lfoRate { 5.0f };       // Hz
    std::atomic<float> lfoDepth { 0.0f };      // 0..1, moves the cutoff and bends the pitch slightly
};

// Four voices, each with two oscillators (saw and square), a low-pass filter and an
// envelope; one LFO shared by all. A fifth note steals the oldest voice.
class PolySynth
{
public:
    void prepare (double newSampleRate) noexcept;
    void noteOn (int note, float gain, int gateSamples) noexcept;
    int activeVoices() const noexcept;
    void setPitch (double ratio) noexcept { pitch = ratio; }
    float next (const SynthParams& params) noexcept;

private:
    struct Voice
    {
        Envelope envelope;
        int note = 0;
        float gain = 1.0f;
        double phase1 = 0.0, phase2 = 0.0;
        float low = 0.0f, band = 0.0f;
        std::uint64_t age = 0;
    };

    double sampleRate = 44100.0;
    std::array<Voice, synthVoices> voices;
    double lfoPhase = 0.0;
    double pitch = 1.0;
    std::uint64_t counter = 0;
};
// Plucked sounds: four voices with an instant attack, a short decay and a filter that
// closes quickly on each note. A fifth note steals the oldest voice.
struct PluckParams
{
    std::atomic<float> cutoff { 1400.0f };   // Hz, opening up to several times that on each note
    std::atomic<float> decay { 0.18f };      // seconds, for the level and the filter
    std::atomic<float> resonance { 0.3f };   // 0..1
    std::atomic<float> osc2Mix { 0.4f };     // 0 = saw only, 1 = square only
};

class PluckSynth
{
public:
    void prepare (double newSampleRate) noexcept;
    void noteOn (int note, float gain, int gateSamples) noexcept;
    int activeVoices() const noexcept;
    void setPitch (double ratio) noexcept { pitch = ratio; }
    float next (const PluckParams& params) noexcept;

private:
    struct Voice
    {
        Envelope envelope;
        int note = 0;
        float gain = 1.0f;
        double phase = 0.0;
        float low = 0.0f, band = 0.0f;
        std::uint64_t age = 0;
    };

    double sampleRate = 44100.0;
    std::array<Voice, pluckVoices> voices;
    double pitch = 1.0;
    std::uint64_t counter = 0;
};
} // namespace drumbox
