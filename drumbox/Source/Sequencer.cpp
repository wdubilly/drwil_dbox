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
}

double Sequencer::stepPosition (std::int64_t absoluteStep) const noexcept
{
    const double shift = (absoluteStep % 2 != 0) ? swing / maxSwingPercent * maxSwingShiftInSteps : 0.0;
    return static_cast<double> (absoluteStep) + shift;
}

void Sequencer::process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out)
{
    const double perStep = samplesPerStep();
    const double end = position + numSamples / perStep;

    while (stepPosition (nextStep) < end)
    {
        const double when = stepPosition (nextStep);
        const int offset = std::clamp (static_cast<int> (std::ceil ((when - position) * perStep)), 0, std::max (numSamples - 1, 0));
        const int step = static_cast<int> (nextStep % numSteps);

        lastStep = step;
        for (int track = 0; track < numTracks; ++track)
        {
            const auto state = pattern.get (track, step);
            if (state != StepState::Off)
                out.push_back ({ offset, track, state == StepState::Accent });
        }
        ++nextStep;
    }

    position = end;
}
} // namespace drumbox
