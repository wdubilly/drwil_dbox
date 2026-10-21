#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace drumbox
{
constexpr int numSteps = 16;
constexpr int numTracks = 8;
constexpr int synthVoices = 4;
constexpr int pluckVoices = 4;
constexpr int numPatterns = 16;
constexpr int maxChainLength = 128;
constexpr int lowestNote = 24;  // C1
constexpr int highestNote = 96; // C7
constexpr int noNote = 0;

enum class StepState : std::uint8_t
{
    Off = 0,
    Normal = 1,
    Accent = 2
};

// Shared between the UI thread (writes) and the audio thread (reads): every cell is atomic.
class Pattern
{
public:
    Pattern() { clear(); }

    void set (int track, int step, StepState state) noexcept
    {
        cells[index (track, step)].store (static_cast<std::uint8_t> (state), std::memory_order_relaxed);
    }

    StepState get (int track, int step) const noexcept
    {
        return static_cast<StepState> (cells[index (track, step)].load (std::memory_order_relaxed));
    }

    // Off -> Normal -> Accent -> Off
    StepState cycle (int track, int step) noexcept
    {
        const auto current = get (track, step);
        const auto next = current == StepState::Off ? StepState::Normal
                        : current == StepState::Normal ? StepState::Accent
                                                       : StepState::Off;
        set (track, step, next);
        return next;
    }

    // Bass: one note per step (monophonic); noNote leaves the step silent.
    void setBass (int step, int note, bool accent = false) noexcept
    {
        bassNotes[(size_t) step].store (clampNote (note), std::memory_order_relaxed);
        bassAccents[(size_t) step].store (accent, std::memory_order_relaxed);
    }
    int getBassNote (int step) const noexcept { return bassNotes[(size_t) step].load (std::memory_order_relaxed); }
    bool getBassAccent (int step) const noexcept { return bassAccents[(size_t) step].load (std::memory_order_relaxed); }

    // Synth: up to synthVoices notes per step, one per slot; noNote leaves the slot empty.
    void setSynthNote (int step, int slot, int note) noexcept
    {
        synthNotes[synthIndex (step, slot)].store (clampNote (note), std::memory_order_relaxed);
    }
    int getSynthNote (int step, int slot) const noexcept
    {
        return synthNotes[synthIndex (step, slot)].load (std::memory_order_relaxed);
    }
    void setSynthAccent (int step, bool accent) noexcept { synthAccents[(size_t) step].store (accent, std::memory_order_relaxed); }
    bool getSynthAccent (int step) const noexcept { return synthAccents[(size_t) step].load (std::memory_order_relaxed); }

    // Pluck: up to pluckVoices notes per step, one per slot; noNote leaves the slot empty.
    void setPluckNote (int step, int slot, int note) noexcept
    {
        pluckNotes[pluckIndex (step, slot)].store (clampNote (note), std::memory_order_relaxed);
    }
    int getPluckNote (int step, int slot) const noexcept
    {
        return pluckNotes[pluckIndex (step, slot)].load (std::memory_order_relaxed);
    }
    void setPluckAccent (int step, bool accent) noexcept { pluckAccents[(size_t) step].store (accent, std::memory_order_relaxed); }
    bool getPluckAccent (int step) const noexcept { return pluckAccents[(size_t) step].load (std::memory_order_relaxed); }

    void clear() noexcept
    {
        for (auto& c : cells)
            c.store (0, std::memory_order_relaxed);
        for (int s = 0; s < numSteps; ++s)
        {
            setBass (s, noNote);
            setSynthAccent (s, false);
            for (int v = 0; v < synthVoices; ++v)
                setSynthNote (s, v, noNote);
            setPluckAccent (s, false);
            for (int v = 0; v < pluckVoices; ++v)
                setPluckNote (s, v, noNote);
        }
    }

private:
    static constexpr int index (int track, int step) noexcept { return track * numSteps + step; }

    static std::uint8_t clampNote (int note) noexcept
    {
        return note == noNote ? std::uint8_t { 0 } : static_cast<std::uint8_t> (std::clamp (note, lowestNote, highestNote));
    }
    static size_t synthIndex (int step, int slot) noexcept { return static_cast<size_t> (step * synthVoices + slot); }

    static size_t pluckIndex (int step, int slot) noexcept { return static_cast<size_t> (step * pluckVoices + slot); }

    std::array<std::atomic<std::uint8_t>, numTracks * numSteps> cells;
    std::array<std::atomic<std::uint8_t>, numSteps> bassNotes;
    std::array<std::atomic<bool>, numSteps> bassAccents;
    std::array<std::atomic<std::uint8_t>, numSteps * synthVoices> synthNotes;
    std::array<std::atomic<bool>, numSteps> synthAccents;
    std::array<std::atomic<std::uint8_t>, numSteps * pluckVoices> pluckNotes;
    std::array<std::atomic<bool>, numSteps> pluckAccents;
};

// Bank of patterns plus the chain that plays them one after the other, then loops.
// A fresh song plays pattern 0 alone, as a single pattern always did.
class Song
{
public:
    Song() { chain[0].store (0); }

    Pattern& pattern (int index) noexcept { return patterns[(size_t) std::clamp (index, 0, numPatterns - 1)]; }
    const Pattern& pattern (int index) const noexcept { return patterns[(size_t) std::clamp (index, 0, numPatterns - 1)]; }

    void setChainLength (int length) noexcept { chainLength.store (std::clamp (length, 1, maxChainLength)); }
    int getChainLength() const noexcept { return chainLength.load(); }
    void setChainEntry (int position, int patternIndex) noexcept
    {
        chain[(size_t) std::clamp (position, 0, maxChainLength - 1)].store (static_cast<std::uint8_t> (std::clamp (patternIndex, 0, numPatterns - 1)));
    }
    int getChainEntry (int position) const noexcept { return chain[(size_t) std::clamp (position, 0, maxChainLength - 1)].load(); }

    // Position in the chain being played at the given absolute step.
    int chainPositionAt (std::int64_t absoluteStep) const noexcept
    {
        return static_cast<int> ((absoluteStep / numSteps) % getChainLength());
    }
    const Pattern& patternAt (std::int64_t absoluteStep) const noexcept
    {
        return pattern (getChainEntry (chainPositionAt (absoluteStep)));
    }

private:
    std::array<Pattern, numPatterns> patterns;
    std::array<std::atomic<std::uint8_t>, maxChainLength> chain {};
    std::atomic<int> chainLength { 1 };
};
} // namespace drumbox
