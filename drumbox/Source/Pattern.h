#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace drumbox
{
constexpr int numSteps = 16;
constexpr int numTracks = 8;

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

    void clear() noexcept
    {
        for (auto& c : cells)
            c.store (0, std::memory_order_relaxed);
    }

private:
    static constexpr int index (int track, int step) noexcept { return track * numSteps + step; }

    std::array<std::atomic<std::uint8_t>, numTracks * numSteps> cells;
};
} // namespace drumbox
