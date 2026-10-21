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
    songPosition = -1;
}

double Sequencer::stepPosition (std::int64_t absoluteStep) const noexcept
{
    const double shift = (absoluteStep % 2 != 0) ? swing / maxSwingPercent * maxSwingShiftInSteps : 0.0;
    return static_cast<double> (absoluteStep) + shift;
}

template <class Locate>
void Sequencer::run (Locate&& locate, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>* notes)
{
    const double perStep = samplesPerStep();
    const double end = position + numSamples / perStep;
    const int gate = std::max (1, static_cast<int> (perStep * gateInSteps));

    while (stepPosition (nextStep) < end)
    {
        const double when = stepPosition (nextStep);
        const int offset = std::clamp (static_cast<int> (std::ceil ((when - position) * perStep)), 0, std::max (numSamples - 1, 0));
        const int step = static_cast<int> (nextStep % numSteps);
        const auto placed = locate (nextStep);
        const Pattern& pattern = placed.pattern;

        lastStep = step;
        lastChainPosition = placed.chainPosition;
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
            for (int v = 0; v < pluckVoices; ++v)
                if (const int note = pattern.getPluckNote (step, v); note != noNote)
                    notes->push_back ({ offset, Instrument::Pluck, note, pattern.getPluckAccent (step), gate });
            for (int row = 0; row < numFxRows; ++row)
                if (const auto state = pattern.getFx (row, step); state != StepState::Off)
                    notes->push_back ({ offset, Instrument::Fx, row, state == StepState::Accent, gate });
        }
        ++nextStep;
    }

    position = end;
}

void Sequencer::process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out)
{
    run ([&pattern] (std::int64_t) { return Placed { pattern, 0 }; }, numSamples, out, nullptr);
}

void Sequencer::process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>& notes)
{
    run ([&pattern] (std::int64_t) { return Placed { pattern, 0 }; }, numSamples, out, &notes);
}

// The position is advanced one pattern at a time rather than computed from the step count, so a
// new chain length applies at once: the music goes on if the position is still inside the chain,
// otherwise it returns to the first position, keeping the beat.
void Sequencer::process (const Song& song, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>& notes)
{
    run ([this, &song] (std::int64_t step) {
            if (songPosition < 0)
                songPosition = 0;
            else if (step % numSteps == 0)
                ++songPosition;
            if (songPosition >= song.getChainLength())
                songPosition = 0;
            return Placed { song.pattern (song.getChainEntry (songPosition)), songPosition };
        },
        numSamples, out, &notes);
}

} // namespace drumbox
