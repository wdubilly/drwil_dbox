#include "Sequencer.h"

#include <algorithm>
#include <cmath>

namespace drumbox
{
void Sequencer::setTempo (double bpm) noexcept
{
    tempo = std::clamp (bpm, minTempo, maxTempo);
}

void Sequencer::setSwing (double percent) noexcept
{
    swing = std::clamp (percent, 0.0, maxSwingPercent);
}

void Sequencer::reset() noexcept
{
    position = 0.0;
    nextStep = 0;
    lastStep = -1;
    lastChainPosition = -1;
}

double Sequencer::stepPosition (std::int64_t absoluteStep) const noexcept
{
    const double shift = (absoluteStep % 2 != 0) ? swing / maxSwingPercent * maxSwingShiftInSteps : 0.0;
    return static_cast<double> (absoluteStep) + shift;
}

template <class PatternAt, class PositionAt>
void Sequencer::run (PatternAt&& patternAt, PositionAt&& positionAt, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>* notes)
{
    const double perStep = samplesPerStep();
    const double end = position + numSamples / perStep;
    const int gate = std::max (1, static_cast<int> (perStep * gateInSteps));

    while (stepPosition (nextStep) < end)
    {
        const double when = stepPosition (nextStep);
        const int offset = std::clamp (static_cast<int> (std::ceil ((when - position) * perStep)), 0, std::max (numSamples - 1, 0));
        const int step = static_cast<int> (nextStep % numSteps);
        const Pattern& pattern = patternAt (nextStep);

        lastStep = step;
        lastChainPosition = positionAt (nextStep);
        for (int track = 0; track < numTracks; ++track)
        {
            const auto state = pattern.get (track, step);
            if (state != StepState::Off)
                out.push_back ({ offset, track, state == StepState::Accent });
        }

        if (notes != nullptr)
        {
            if (const int bass = pattern.getBassNote (step); bass != noNote)
                notes->push_back ({ offset, Instrument::Bass, bass, pattern.getBassAccent (step), gate });
            for (int v = 0; v < synthVoices; ++v)
                if (const int note = pattern.getSynthNote (step, v); note != noNote)
                    notes->push_back ({ offset, Instrument::Synth, note, pattern.getSynthAccent (step), gate });
        }
        ++nextStep;
    }

    position = end;
}

void Sequencer::process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out)
{
    run ([&pattern] (std::int64_t) -> const Pattern& { return pattern; }, [] (std::int64_t) { return 0; }, numSamples, out, nullptr);
}

void Sequencer::process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>& notes)
{
    run ([&pattern] (std::int64_t) -> const Pattern& { return pattern; }, [] (std::int64_t) { return 0; }, numSamples, out, &notes);
}

void Sequencer::process (const Song& song, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>& notes)
{
    run ([&song] (std::int64_t step) -> const Pattern& { return song.patternAt (step); },
         [&song] (std::int64_t step) { return song.chainPositionAt (step); }, numSamples, out, &notes);
}

} // namespace drumbox
