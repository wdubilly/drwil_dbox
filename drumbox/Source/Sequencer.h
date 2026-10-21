#pragma once

#include "Pattern.h"

#include <cstdint>
#include <vector>

namespace drumbox
{
struct Trigger
{
    int sampleOffset; // within the block being processed
    int track;
    bool accent;
};

enum class Instrument : int
{
    Bass = 0,
    Synth
};

struct NoteEvent
{
    int sampleOffset; // within the block being processed
    Instrument instrument;
    int note;
    bool accent;
    int gateSamples; // how long the key is held before its release
};

// Steps are sixteenth notes. Time is tracked in step units so a tempo change
// mid-play takes effect without a jump. Odd steps are delayed by swing:
// 0% is a straight grid, 100% delays them by half a step.
class Sequencer
{
public:
    static constexpr double minTempo = 40.0;
    static constexpr double maxTempo = 240.0;
    static constexpr double maxSwingPercent = 100.0;
    static constexpr double maxSwingShiftInSteps = 0.5;

    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }
    void setTempo (double bpm) noexcept;
    void setSwing (double percent) noexcept;
    double getTempo() const noexcept { return tempo; }
    double getSwing() const noexcept { return swing; }

    void reset() noexcept;
    // Step index (0-15) of the last step reached, -1 before the first one.
    int getLastStep() const noexcept { return lastStep; }
    // Chain position of the last step reached, -1 before the first one.
    int getLastChainPosition() const noexcept { return lastChainPosition; }

    // Appends the triggers falling inside the next numSamples and advances the clock.
    void process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out);
    // Same, also appending the bass and synth notes; one pattern played alone.
    void process (const Pattern& pattern, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>& notes);
    // Plays the patterns of the song in chain order, then loops.
    void process (const Song& song, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>& notes);

    double samplesPerStep() const noexcept { return sampleRate * 60.0 / tempo / 4.0; }
    // Position in steps of the trigger of the given absolute step number.
    double stepPosition (std::int64_t absoluteStep) const noexcept;

    static constexpr double gateInSteps = 0.8;

private:
    template <class PatternAt, class PositionAt>
    void run (PatternAt&& patternAt, PositionAt&& positionAt, int numSamples, std::vector<Trigger>& out, std::vector<NoteEvent>* notes);

    double sampleRate = 44100.0;
    double tempo = 120.0;
    double swing = 0.0;
    double position = 0.0; // in steps
    std::int64_t nextStep = 0;
    int lastStep = -1;
    int lastChainPosition = -1;
};
} // namespace drumbox
