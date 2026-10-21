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
    sequencer.reset();
    wasPlaying = false;
    currentStep.store (-1);
    currentChainPosition.store (-1);
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
    for (int t = 0; t < numMixerTracks; ++t)
    {
        target[static_cast<size_t> (t)] = mixer.targetGain (t, soloActive);
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
        mix += bass.next (bassParams) * bassLevel * trackGain[mixerBass] + synth.next (synthParams) * synthLevel * trackGain[mixerPad]
               + pluck.next (pluckParams) * pluckLevel * trackGain[mixerPluck] + fx.next() * fxLevel * trackGain[mixerFx];
        out[i] = std::clamp (mix, -1.0f, 1.0f);
    }
}
} // namespace drumbox
