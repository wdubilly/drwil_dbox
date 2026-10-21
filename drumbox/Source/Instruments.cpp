#include "Instruments.h"

#include <algorithm>
#include <cmath>

namespace drumbox
{
namespace
{
constexpr double pi = 3.141592653589793;
constexpr double twoPi = 2.0 * pi;

// Band-limiting correction for the edges of a naive saw or square.
double polyBlep (double t, double dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

float saw (double phase, double dt) noexcept
{
    return static_cast<float> (2.0 * phase - 1.0 - polyBlep (phase, dt));
}

float square (double phase, double dt) noexcept
{
    double wrapped = phase + 0.5;
    wrapped -= std::floor (wrapped);
    return static_cast<float> ((phase < 0.5 ? 1.0 : -1.0) + polyBlep (phase, dt) - polyBlep (wrapped, dt));
}

double advance (double phase, double dt) noexcept
{
    phase += dt;
    return phase - std::floor (phase);
}

// State-variable low-pass (trapezoidal): stable up to high cutoffs and resonances.
float lowPass (float input, float& low, float& band, double cutoff, double resonance, double sampleRate) noexcept
{
    const double fc = std::clamp (cutoff, 20.0, sampleRate * 0.45);
    const double g = std::tan (pi * fc / sampleRate);
    const double k = 2.0 - 1.9 * std::clamp (resonance, 0.0, 1.0);
    const double a1 = 1.0 / (1.0 + g * (g + k));
    const double a2 = g * a1;
    const double a3 = g * a2;
    const double v3 = input - low;
    const double v1 = a1 * band + a2 * v3;
    const double v2 = low + a2 * band + a3 * v3;
    band = static_cast<float> (2.0 * v1 - band);
    low = static_cast<float> (2.0 * v2 - low);
    return static_cast<float> (v2);
}
} // namespace

double noteToFrequency (int note) noexcept
{
    return 440.0 * std::pow (2.0, (note - 69) / 12.0);
}

void Envelope::noteOn (int gateSamples) noexcept
{
    stage = Stage::Attack;
    gateLeft = std::max (gateSamples, 1);
}

void Envelope::noteOff() noexcept
{
    if (stage == Stage::Idle || stage == Stage::Release)
        return;
    stage = Stage::Release;
    releaseStep = -1.0f; // computed on the next sample, once the release time is known
}

float Envelope::next (const Settings& s, double sampleRate) noexcept
{
    if (stage == Stage::Idle)
        return 0.0f;

    if (isHeld() && --gateLeft <= 0)
        noteOff();

    const double sr = sampleRate;
    switch (stage)
    {
        case Stage::Attack:
            level += static_cast<float> (1.0 / std::max (s.attack * sr, 1.0));
            if (level >= 1.0f)
            {
                level = 1.0f;
                stage = Stage::Decay;
            }
            break;
        case Stage::Decay:
            level -= static_cast<float> ((1.0 - s.sustain) / std::max (s.decay * sr, 1.0));
            if (level <= s.sustain)
            {
                level = s.sustain;
                stage = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            level = s.sustain;
            break;
        case Stage::Release:
            if (releaseStep < 0.0f)
                releaseStep = level / static_cast<float> (std::max (s.release * sr, 1.0));
            level -= releaseStep;
            if (level <= 0.0f)
            {
                level = 0.0f;
                stage = Stage::Idle;
            }
            break;
        case Stage::Idle:
            break;
    }
    return level;
}

void BassVoice::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    envelope = {};
    low = band = 0.0f;
}

void BassVoice::noteOn (int note, float newGain, int gateSamples) noexcept
{
    frequency = noteToFrequency (note);
    gain = newGain;
    envelope.noteOn (gateSamples);
}

float BassVoice::next (const BassParams& params) noexcept
{
    if (! envelope.isActive())
        return 0.0f;

    const float decay = params.decay.load (std::memory_order_relaxed);
    const float level = envelope.next ({ 0.003f, decay, 0.55f, 0.06f }, sampleRate);

    const double dt = frequency / sampleRate;
    phase = advance (phase, dt);
    const float raw = 0.6f * saw (phase, dt) + 0.4f * square (phase, dt);

    // the filter opens on each note and closes with the envelope: the plucked funk bass
    const double cutoff = params.cutoff.load (std::memory_order_relaxed) * (1.0 + 3.0 * level);
    return lowPass (raw, low, band, cutoff, 0.25, sampleRate) * level * gain;
}

void PolySynth::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    for (auto& v : voices)
        v = {};
    lfoPhase = 0.0;
    counter = 0;
}

int PolySynth::activeVoices() const noexcept
{
    int n = 0;
    for (auto& v : voices)
        n += v.envelope.isActive() ? 1 : 0;
    return n;
}

void PolySynth::noteOn (int note, float gain, int gateSamples) noexcept
{
    Voice* target = nullptr;
    for (auto& v : voices)
        if (v.envelope.isActive() && v.note == note)
            target = &v;
    for (auto& v : voices)
        if (target == nullptr && ! v.envelope.isActive())
            target = &v;
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.age < target->age)
                target = &v;
    }

    const bool fresh = ! target->envelope.isActive();
    target->note = note;
    target->gain = gain;
    target->age = ++counter;
    if (fresh)
    {
        target->phase1 = target->phase2 = 0.0;
        target->low = target->band = 0.0f;
    }
    target->envelope.noteOn (gateSamples);
}

float PolySynth::next (const SynthParams& params) noexcept
{
    const auto load = [] (const std::atomic<float>& value) { return value.load (std::memory_order_relaxed); };
    const Envelope::Settings settings { load (params.attack), load (params.decay), load (params.sustain), load (params.release) };
    const double depth = load (params.lfoDepth);
    const double semitones2 = load (params.osc2Semitones);
    const double detune = load (params.osc2Detune);
    const float mix = std::clamp (load (params.osc2Mix), 0.0f, 1.0f);

    lfoPhase = advance (lfoPhase, load (params.lfoRate) / sampleRate);
    const double lfo = std::sin (twoPi * lfoPhase);
    const double cutoff = load (params.cutoff) * std::pow (2.0, 2.0 * depth * lfo);
    const double bend = std::pow (2.0, depth * lfo * 0.25 / 12.0);

    float sum = 0.0f;
    for (auto& v : voices)
    {
        if (! v.envelope.isActive())
            continue;
        const float level = v.envelope.next (settings, sampleRate);

        const double f1 = noteToFrequency (v.note) * bend;
        const double f2 = f1 * std::pow (2.0, semitones2 / 12.0 + detune / 1200.0);
        v.phase1 = advance (v.phase1, f1 / sampleRate);
        v.phase2 = advance (v.phase2, f2 / sampleRate);
        const float raw = (1.0f - mix) * saw (v.phase1, f1 / sampleRate) + mix * square (v.phase2, f2 / sampleRate);
        sum += lowPass (raw, v.low, v.band, cutoff, load (params.resonance), sampleRate) * level * v.gain;
    }
    return sum * 0.5f;
}
} // namespace drumbox
