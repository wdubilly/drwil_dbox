#include "Voices.h"

#include <cmath>

namespace drumbox
{
namespace
{
constexpr double twoPi = 6.283185307179586;

float decay (double time, double seconds) noexcept
{
    // reaches about -60 dB after `seconds`
    return static_cast<float> (std::exp (-6.9 * time / seconds));
}
} // namespace

void Voice::prepare (double newSampleRate, Track newTrack) noexcept
{
    sampleRate = newSampleRate;
    track = newTrack;
    active = false;
    seed = 22222u + static_cast<std::uint32_t> (newTrack) * 7919u;
}

void Voice::trigger (float newGain) noexcept
{
    gain = newGain;
    time = 0.0;
    phaseA = phaseB = phaseC = 0.0;
    highpassState = bandState1 = bandState2 = 0.0f;
    active = true;
}

float Voice::noise() noexcept
{
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float> (static_cast<std::int32_t> (seed)) / 2147483648.0f;
}

float Voice::next() noexcept
{
    if (! active)
        return 0.0f;

    const double dt = 1.0 / sampleRate;
    float out = 0.0f;
    float envelope = 0.0f;
    double length = 0.5;

    switch (track)
    {
        case Kick:
        {
            const double freq = 48.0 + 110.0 * std::exp (-time * 28.0);
            phaseA += freq * dt;
            envelope = decay (time, 0.45);
            out = static_cast<float> (std::sin (twoPi * phaseA)) * envelope;
            length = 0.5;
            break;
        }
        case Snare:
        {
            phaseA += (185.0 + 60.0 * std::exp (-time * 60.0)) * dt;
            const float tone = static_cast<float> (std::sin (twoPi * phaseA)) * decay (time, 0.12);
            const float n = noise();
            const float hp = n - highpassState;
            highpassState += 0.35f * hp;
            out = 0.55f * tone + 0.75f * hp * decay (time, 0.2);
            length = 0.25;
            break;
        }
        case ClosedHat:
        case OpenHat:
        {
            phaseA += 330.0 * dt;
            phaseB += 477.0 * dt;
            phaseC += 631.0 * dt;
            const float metal = 0.3f * (square (phaseA - std::floor (phaseA)) + square (phaseB - std::floor (phaseB)) + square (phaseC - std::floor (phaseC)));
            const float n = noise();
            const float hp = (metal + n) - highpassState;
            highpassState += 0.15f * hp;
            const double seconds = track == ClosedHat ? 0.06 : 0.45;
            out = 0.5f * hp * decay (time, seconds);
            length = seconds;
            break;
        }
        case Clap:
        {
            // three short bursts then a longer tail, through a crude band-pass
            const double t = std::fmod (time, 0.011);
            const float burst = time < 0.033 ? decay (t, 0.009) : decay (time - 0.033, 0.16);
            const float n = noise();
            bandState1 += 0.45f * (n - bandState1);
            bandState2 += 0.12f * (bandState1 - bandState2);
            out = 2.2f * (bandState1 - bandState2) * burst;
            length = 0.3;
            break;
        }
        case Tom:
        {
            phaseA += (105.0 + 90.0 * std::exp (-time * 14.0)) * dt;
            out = static_cast<float> (std::sin (twoPi * phaseA)) * decay (time, 0.35);
            length = 0.4;
            break;
        }
        case Rim:
        {
            phaseA += 1750.0 * dt;
            phaseB += 480.0 * dt;
            const float click = 0.5f * static_cast<float> (std::sin (twoPi * phaseA)) + 0.5f * static_cast<float> (std::sin (twoPi * phaseB));
            out = (click + 0.25f * noise()) * decay (time, 0.03);
            length = 0.05;
            break;
        }
        case Cowbell:
        {
            phaseA += 587.0 * dt;
            phaseB += 845.0 * dt;
            const float pair = 0.5f * (square (phaseA - std::floor (phaseA)) + square (phaseB - std::floor (phaseB)));
            bandState1 += 0.25f * (pair - bandState1);
            out = 0.8f * bandState1 * decay (time, 0.3);
            length = 0.35;
            break;
        }
    }

    time += dt;
    if (time > length)
    {
        active = false;
        level = 0.0f;
    }
    return out * gain;
}

void VoiceBank::prepare (double sampleRate) noexcept
{
    for (int i = 0; i < 8; ++i)
        voices[static_cast<size_t> (i)].prepare (sampleRate, static_cast<Track> (i));
}

void VoiceBank::trigger (int track, float gain) noexcept
{
    if (track < 0 || track >= 8)
        return;

    // a closed hat cuts the open hat, as on the originals
    if (track == ClosedHat)
        voices[OpenHat].choke();

    voices[static_cast<size_t> (track)].trigger (gain);
}

float VoiceBank::next() noexcept
{
    float sum = 0.0f;
    for (auto& v : voices)
        sum += v.next();
    return sum;
}
} // namespace drumbox
