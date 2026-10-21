#pragma once

#include <array>
#include <cstdint>

namespace drumbox
{
enum Track : int
{
    Kick = 0,
    Snare,
    ClosedHat,
    OpenHat,
    Clap,
    Tom,
    Rim,
    Cowbell
};

constexpr std::array<const char*, 8> trackNames { "KICK", "SNARE", "C.HAT", "O.HAT", "CLAP", "TOM", "RIM", "COWBELL" };

// One monophonic synthesised drum voice; retriggering restarts it.
class Voice
{
public:
    void prepare (double newSampleRate, Track newTrack) noexcept;
    void trigger (float gain) noexcept;
    void choke() noexcept { level = 0.0f; active = false; }
    bool isActive() const noexcept { return active; }
    float next() noexcept;

private:
    float noise() noexcept;
    float square (double phase) const noexcept { return phase < 0.5 ? 1.0f : -1.0f; }

    Track track = Kick;
    double sampleRate = 44100.0;
    bool active = false;
    float level = 0.0f;
    float gain = 1.0f;
    std::uint32_t seed = 22222u;
    double time = 0.0; // seconds since trigger
    double phaseA = 0.0, phaseB = 0.0, phaseC = 0.0;
    float highpassState = 0.0f;
    float bandState1 = 0.0f, bandState2 = 0.0f;
};

class VoiceBank
{
public:
    void prepare (double sampleRate) noexcept;
    void trigger (int track, float gain) noexcept;
    float next() noexcept;

private:
    std::array<Voice, 8> voices;
};
} // namespace drumbox
