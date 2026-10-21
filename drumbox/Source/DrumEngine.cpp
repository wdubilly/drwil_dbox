#include "DrumEngine.h"

#include <algorithm>

namespace drumbox
{
namespace
{
constexpr float gainStep = 1.0f / 128.0f;
} // namespace

DrumEngine::DrumEngine()
{
    trackGain.fill (1.0f);
    triggers.reserve (256);
    notes.reserve (256);
    prepare (44100.0);
}

void DrumEngine::prepare (double sampleRate)
{
    sequencer.prepare (sampleRate);
    voices.prepare (sampleRate);
    fx.prepare (sampleRate);
    bass.prepare (sampleRate);
    synth.prepare (sampleRate);
    pluck.prepare (sampleRate);
    effects.prepare (sampleRate);
    sequencer.reset();
    wasPlaying = false;
    currentStep.store (-1);
    currentChainPosition.store (-1);
}

void DrumEngine::settleMixer() noexcept
{
    const bool soloActive = mixer.anySolo();
    for (int t = 0; t < numMixerTracks; ++t)
        trackGain[static_cast<size_t> (t)] = mixer.targetGain (t, soloActive);
}

void DrumEngine::render (float* out, int numSamples)
{
    sequencer.setTempo (tempoRequest.load());
    sequencer.setSwing (swingRequest.load());

    const bool playing = playRequest.load();
    if (playing && ! wasPlaying)
        sequencer.reset();
    if (! playing && wasPlaying)
    {
        currentStep.store (-1);
        currentChainPosition.store (-1);
    }
    wasPlaying = playing;

    triggers.clear();
    notes.clear();
    if (playing)
    {
        sequencer.process (song, numSamples, triggers, notes);
        currentStep.store (sequencer.getLastStep());
        currentChainPosition.store (sequencer.getLastChainPosition());
    }

    const bool soloActive = mixer.anySolo();
    std::array<float, numMixerTracks> target;
    std::array<float, numMixerTracks> sendGain;
    for (int t = 0; t < numMixerTracks; ++t)
    {
        target[static_cast<size_t> (t)] = mixer.targetGain (t, soloActive);
        sendGain[static_cast<size_t> (t)] = mixer.getSend (t);
        const double ratio = mixer.pitchRatio (t);
        if (t < mixerDrums)
            voices.setPitch (t, ratio);
        else if (t == mixerBass)
            bass.setPitch (ratio);
        else if (t == mixerPad)
            synth.setPitch (ratio);
        else if (t == mixerPluck)
            pluck.setPitch (ratio);
        else
            fx.setPitch (ratio);
    }

    // triggers and notes are ordered by offset; voices keep ringing after a stop
    size_t next = 0;
    size_t nextNote = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        while (next < triggers.size() && triggers[next].sampleOffset <= i)
        {
            voices.trigger (triggers[next].track, triggers[next].accent ? accentGain : normalGain);
            ++next;
        }
        while (nextNote < notes.size() && notes[nextNote].sampleOffset <= i)
        {
            const auto& n = notes[nextNote];
            const float gain = n.accent ? accentGain : normalGain;
            if (n.instrument == Instrument::Bass)
                bass.noteOn (n.note, gain, n.gateSamples);
            else if (n.instrument == Instrument::Synth)
                synth.noteOn (n.note, gain, n.gateSamples);
            else if (n.instrument == Instrument::Pluck)
                pluck.noteOn (n.note, gain, n.gateSamples);
            else
                fx.trigger (n.note, gain);
            ++nextNote;
        }
        // gains glide to their target in about 3 ms, so a mute or a solo does not click
        for (size_t t = 0; t < trackGain.size(); ++t)
            trackGain[t] += std::clamp (target[t] - trackGain[t], -gainStep, gainStep);

        std::array<float, mixerDrums> drumOut;
        voices.next (drumOut);
        float mix = 0.0f;
        for (size_t d = 0; d < drumOut.size(); ++d)
            mix += drumOut[d] * 0.6f * trackGain[d];
        const float bassOut = bass.next (bassParams) * bassLevel * trackGain[mixerBass];
        const float padOut = synth.next (synthParams) * synthLevel * trackGain[mixerPad];
        const float pluckOut = pluck.next (pluckParams) * pluckLevel * trackGain[mixerPluck];
        const float fxOut = fx.next() * fxLevel * trackGain[mixerFx];
        mix += bassOut + padOut + pluckOut + fxOut;

        // the chain takes the dry mix plus each track's send; with every effect off it is bypassed
        if (effects.anyOn())
        {
            float sends = 0.0f;
            for (size_t d = 0; d < drumOut.size(); ++d)
                sends += drumOut[d] * 0.6f * trackGain[d] * sendGain[d];
            sends += bassOut * sendGain[mixerBass] + padOut * sendGain[mixerPad] + pluckOut * sendGain[mixerPluck]
                     + fxOut * sendGain[mixerFx];
            mix = effects.process (mix + sends);
        }
        out[i] = std::clamp (mix, -1.0f, 1.0f);
    }
}
} // namespace drumbox
