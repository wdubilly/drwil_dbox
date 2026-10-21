#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace drumbox
{
constexpr int numSteps = 16;
constexpr int numTracks = 8;
constexpr int numFxRows = 6;
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

// Plain copy of everything a pattern holds, for the clipboard and the swap.
struct PatternData
{
    std::array<std::uint8_t, numTracks * numSteps> cells {};
    std::array<std::uint8_t, numFxRows * numSteps> fxCells {};
    std::array<std::uint8_t, numSteps> bassNotes {};
    std::array<bool, numSteps> bassAccents {};
    std::array<std::uint8_t, numSteps * synthVoices> synthNotes {};
    std::array<bool, numSteps> synthAccents {};
    std::array<std::uint8_t, numSteps * pluckVoices> pluckNotes {};
    std::array<bool, numSteps> pluckAccents {};
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

    // FX: one off/normal/accent step row per sound effect.
    void setFx (int row, int step, StepState state) noexcept
    {
        fxCells[fxIndex (row, step)].store (static_cast<std::uint8_t> (state), std::memory_order_relaxed);
    }
    StepState getFx (int row, int step) const noexcept
    {
        return static_cast<StepState> (fxCells[fxIndex (row, step)].load (std::memory_order_relaxed));
    }
    StepState cycleFx (int row, int step) noexcept
    {
        const auto current = getFx (row, step);
        const auto next = current == StepState::Off ? StepState::Normal
                        : current == StepState::Normal ? StepState::Accent
                                                       : StepState::Off;
        setFx (row, step, next);
        return next;
    }

    PatternData capture() const noexcept
    {
        PatternData d;
        copyOut (cells, d.cells);
        copyOut (fxCells, d.fxCells);
        copyOut (bassNotes, d.bassNotes);
        copyOut (bassAccents, d.bassAccents);
        copyOut (synthNotes, d.synthNotes);
        copyOut (synthAccents, d.synthAccents);
        copyOut (pluckNotes, d.pluckNotes);
        copyOut (pluckAccents, d.pluckAccents);
        return d;
    }

    void apply (const PatternData& d) noexcept
    {
        copyIn (d.cells, cells);
        copyIn (d.fxCells, fxCells);
        copyIn (d.bassNotes, bassNotes);
        copyIn (d.bassAccents, bassAccents);
        copyIn (d.synthNotes, synthNotes);
        copyIn (d.synthAccents, synthAccents);
        copyIn (d.pluckNotes, pluckNotes);
        copyIn (d.pluckAccents, pluckAccents);
    }

    void clear() noexcept
    {
        for (auto& c : cells)
            c.store (0, std::memory_order_relaxed);
        for (auto& c : fxCells)
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
    template <class Atomics, class Plain>
    static void copyOut (const Atomics& from, Plain& to) noexcept
    {
        for (size_t i = 0; i < from.size(); ++i)
            to[i] = from[i].load (std::memory_order_relaxed);
    }
    template <class Plain, class Atomics>
    static void copyIn (const Plain& from, Atomics& to) noexcept
    {
        for (size_t i = 0; i < from.size(); ++i)
            to[i].store (from[i], std::memory_order_relaxed);
    }

    static constexpr int index (int track, int step) noexcept { return track * numSteps + step; }

    static size_t fxIndex (int row, int step) noexcept { return static_cast<size_t> (std::clamp (row, 0, numFxRows - 1) * numSteps + step); }

    static std::uint8_t clampNote (int note) noexcept
    {
        return note == noNote ? std::uint8_t { 0 } : static_cast<std::uint8_t> (std::clamp (note, lowestNote, highestNote));
    }
    static size_t synthIndex (int step, int slot) noexcept { return static_cast<size_t> (step * synthVoices + slot); }

    static size_t pluckIndex (int step, int slot) noexcept { return static_cast<size_t> (step * pluckVoices + slot); }

    std::array<std::atomic<std::uint8_t>, numTracks * numSteps> cells;
    std::array<std::atomic<std::uint8_t>, numFxRows * numSteps> fxCells;
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

    // Pattern operations: each touches only the patterns it names. An index out of range is brought back into it.
    PatternData copyPattern (int index) const noexcept { return pattern (index).capture(); }
    void pastePattern (int index, const PatternData& data) noexcept { pattern (index).apply (data); }
    void clearPattern (int index) noexcept { pattern (index).clear(); }
    void swapPatterns (int a, int b) noexcept
    {
        if (std::clamp (a, 0, numPatterns - 1) == std::clamp (b, 0, numPatterns - 1))
            return;
        const auto first = pattern (a).capture();
        pattern (a).apply (pattern (b).capture());
        pattern (b).apply (first);
    }

    void setChainLength (int length) noexcept { chainLength.store (std::clamp (length, 1, maxChainLength)); }
    int getChainLength() const noexcept { return chainLength.load(); }
    void setChainEntry (int position, int patternIndex) noexcept
    {
        chain[(size_t) std::clamp (position, 0, maxChainLength - 1)].store (static_cast<std::uint8_t> (std::clamp (patternIndex, 0, numPatterns - 1)));
    }
    int getChainEntry (int position) const noexcept { return chain[(size_t) std::clamp (position, 0, maxChainLength - 1)].load(); }


private:
    std::array<Pattern, numPatterns> patterns;
    std::array<std::atomic<std::uint8_t>, maxChainLength> chain {};
    std::atomic<int> chainLength { 1 };
};
} // namespace drumbox
