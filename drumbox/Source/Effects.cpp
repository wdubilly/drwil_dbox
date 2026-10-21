#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace drumbox
{
namespace
{
constexpr double pi = 3.14159265358979323846;

size_t scaled (int samplesAt44100, double sampleRate)
{
    return std::max<size_t> (1, static_cast<size_t> (samplesAt44100 * sampleRate / 44100.0));
}

bool isOn (const std::atomic<float>& on) noexcept { return on.load() >= 0.5f; }
} // namespace

void Reverb::prepare (double sampleRate)
{
    constexpr int combLengths[numCombs] = { 1116, 1188, 1277, 1356 };
    constexpr int allpassLengths[numAllpasses] = { 556, 441 };
    for (int i = 0; i < numCombs; ++i)
        combs[static_cast<size_t> (i)].assign (scaled (combLengths[i], sampleRate), 0.0f);
    for (int i = 0; i < numAllpasses; ++i)
        allpasses[static_cast<size_t> (i)].assign (scaled (allpassLengths[i], sampleRate), 0.0f);
    combPos.fill (0);
    combLow.fill (0.0f);
    allpassPos.fill (0);
}

float Reverb::process (float in, const ReverbParams& params) noexcept
{
    const float feedback = 0.7f + 0.28f * std::clamp (params.size.load(), 0.0f, 1.0f);
    const float damp = std::clamp (params.damp.load(), 0.0f, 1.0f) * 0.8f;
    const float mix = std::clamp (params.mix.load(), 0.0f, 1.0f);

    float wet = 0.0f;
    for (size_t i = 0; i < numCombs; ++i)
    {
        auto& line = combs[i];
        const float out = line[combPos[i]];
        combLow[i] = out * (1.0f - damp) + combLow[i] * damp;
        line[combPos[i]] = in * 0.15f + combLow[i] * feedback;
        combPos[i] = (combPos[i] + 1) % line.size();
        wet += out;
    }
    for (size_t i = 0; i < numAllpasses; ++i)
    {
        auto& line = allpasses[i];
        const float buffered = line[allpassPos[i]];
        const float out = buffered - wet;
        line[allpassPos[i]] = wet + buffered * 0.5f;
        allpassPos[i] = (allpassPos[i] + 1) % line.size();
        wet = out;
    }
    return in + wet * mix * 1.5f;
}

void Delay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    buffer.assign (static_cast<size_t> (sampleRate * 1.05) + 2, 0.0f);
    pos = 0;
}

float Delay::process (float in, const DelayParams& params) noexcept
{
    const double ms = std::clamp (static_cast<double> (params.timeMs.load()), 20.0, 1000.0);
    const size_t delaySamples = std::min (buffer.size() - 1, static_cast<size_t> (ms * sampleRate / 1000.0));
    const float feedback = std::clamp (params.feedback.load(), 0.0f, 0.9f);
    const float mix = std::clamp (params.mix.load(), 0.0f, 1.0f);

    const float echo = buffer[(pos + buffer.size() - delaySamples) % buffer.size()];
    buffer[pos] = in + echo * feedback;
    pos = (pos + 1) % buffer.size();
    return in + echo * mix;
}

float Distortion::process (float in, const DistortionParams& params) noexcept
{
    const float drive = std::clamp (params.drive.load(), 0.0f, 1.0f);
    const float tone = std::clamp (params.tone.load(), 0.0f, 1.0f);
    const float mix = std::clamp (params.mix.load(), 0.0f, 1.0f);

    const float gain = 1.0f + drive * 24.0f;
    const float clipped = std::tanh (in * gain) * 0.6f;
    const double cutoff = 800.0 + tone * tone * 11000.0;
    const float a = static_cast<float> (1.0 - std::exp (-2.0 * pi * cutoff / sampleRate));
    low += a * (clipped - low);
    return in * (1.0f - mix) + low * mix;
}

void Chorus::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    buffer.assign (static_cast<size_t> (sampleRate * 0.05) + 2, 0.0f);
    pos = 0;
    phase = 0.0;
}

float Chorus::process (float in, const ChorusParams& params) noexcept
{
    const double rate = std::clamp (static_cast<double> (params.rate.load()), 0.1, 5.0);
    const double depth = std::clamp (static_cast<double> (params.depth.load()), 0.0, 1.0);
    const float mix = std::clamp (params.mix.load(), 0.0f, 1.0f);

    buffer[pos] = in;
    phase += rate / sampleRate;
    phase -= std::floor (phase);
    const double delaySamples = (0.012 + 0.006 * depth * std::sin (2.0 * pi * phase)) * sampleRate;
    const double readAt = static_cast<double> (pos + buffer.size()) - delaySamples;
    const size_t i0 = static_cast<size_t> (readAt) % buffer.size();
    const size_t i1 = (i0 + 1) % buffer.size();
    const float frac = static_cast<float> (readAt - std::floor (readAt));
    const float wet = buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
    pos = (pos + 1) % buffer.size();
    return in * (1.0f - mix * 0.5f) + wet * mix;
}

float Compressor::process (float in, const CompressorParams& params) noexcept
{
    const float threshold = std::clamp (params.threshold.load(), -48.0f, 0.0f);
    const float ratio = std::clamp (params.ratio.load(), 1.0f, 20.0f);
    const float makeup = std::clamp (params.makeup.load(), 0.0f, 18.0f);

    const float level = std::fabs (in);
    const double seconds = level > envelope ? 0.002 : 0.12;
    const float coeff = static_cast<float> (std::exp (-1.0 / (seconds * sampleRate)));
    envelope = level + coeff * (envelope - level);

    const float levelDb = 20.0f * std::log10 (std::max (envelope, 1.0e-6f));
    const float over = std::max (0.0f, levelDb - threshold);
    const float gainDb = makeup - over * (1.0f - 1.0f / ratio);
    return in * std::pow (10.0f, gainDb / 20.0f);
}

void EffectsRack::prepare (double sampleRate)
{
    reverbFx.prepare (sampleRate);
    delayFx.prepare (sampleRate);
    distortionFx.prepare (sampleRate);
    chorusFx.prepare (sampleRate);
    compressorFx.prepare (sampleRate);
}

const char* EffectsRack::effectName (int effect) noexcept
{
    static constexpr const char* names[numEffects] = { "REVERB", "DELAY", "DISTORTION", "CHORUS", "COMPRESSOR" };
    return effect >= 0 && effect < numEffects ? names[effect] : "";
}

std::vector<EffectSetting> EffectsRack::settings (int effect)
{
    switch (effect)
    {
        case 0:
            return { { "on", &reverbParams.on, 0.0f, 1.0f }, { "size", &reverbParams.size, 0.0f, 1.0f },
                     { "damp", &reverbParams.damp, 0.0f, 1.0f }, { "mix", &reverbParams.mix, 0.0f, 1.0f } };
        case 1:
            return { { "on", &delayParams.on, 0.0f, 1.0f }, { "time", &delayParams.timeMs, 20.0f, 1000.0f },
                     { "feedback", &delayParams.feedback, 0.0f, 0.9f }, { "mix", &delayParams.mix, 0.0f, 1.0f } };
        case 2:
            return { { "on", &distortionParams.on, 0.0f, 1.0f }, { "drive", &distortionParams.drive, 0.0f, 1.0f },
                     { "tone", &distortionParams.tone, 0.0f, 1.0f }, { "mix", &distortionParams.mix, 0.0f, 1.0f } };
        case 3:
            return { { "on", &chorusParams.on, 0.0f, 1.0f }, { "rate", &chorusParams.rate, 0.1f, 5.0f },
                     { "depth", &chorusParams.depth, 0.0f, 1.0f }, { "mix", &chorusParams.mix, 0.0f, 1.0f } };
        case 4:
            return { { "on", &compressorParams.on, 0.0f, 1.0f }, { "threshold", &compressorParams.threshold, -48.0f, 0.0f },
                     { "ratio", &compressorParams.ratio, 1.0f, 20.0f }, { "makeup", &compressorParams.makeup, 0.0f, 18.0f } };
        default:
            return {};
    }
}

bool EffectsRack::anyOn() const noexcept
{
    return isOn (reverbParams.on) || isOn (delayParams.on) || isOn (distortionParams.on) || isOn (chorusParams.on)
           || isOn (compressorParams.on);
}

float EffectsRack::process (float in) noexcept
{
    float x = in;
    if (isOn (distortionParams.on))
        x = distortionFx.process (x, distortionParams);
    if (isOn (chorusParams.on))
        x = chorusFx.process (x, chorusParams);
    if (isOn (compressorParams.on))
        x = compressorFx.process (x, compressorParams);
    if (isOn (delayParams.on))
        x = delayFx.process (x, delayParams);
    if (isOn (reverbParams.on))
        x = reverbFx.process (x, reverbParams);
    return x;
}
} // namespace drumbox
