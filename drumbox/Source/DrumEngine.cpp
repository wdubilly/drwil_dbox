#include "DrumEngine.h"

#include <algorithm>

namespace drumbox
{
DrumEngine::DrumEngine()
{
    triggers.reserve (256);
    notes.reserve (256);
    prepare (44100.0);
}

void DrumEngine::prepare (double sampleRate)
{
    sequencer.prepare (sampleRate);
    voices.prepare (sampleRate);
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

    // triggers are ordered by offset; voices keep ringing after a stop
    size_t next = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        while (next < triggers.size() && triggers[next].sampleOffset <= i)
        {
            voices.trigger (triggers[next].track, triggers[next].accent ? accentGain : normalGain);
            ++next;
        }
        out[i] = std::clamp (voices.next() * 0.6f, -1.0f, 1.0f);
    }
}
} // namespace drumbox
