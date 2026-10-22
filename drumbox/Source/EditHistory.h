#pragma once

#include "DrumEngine.h"

#include <cstddef>
#include <deque>
#include <string>

namespace drumbox
{
// Undo and redo over the whole song, engine included: every state is the song file text, so what an undo
// brings back is exactly what a save and an open would. Not for the audio thread.
class EditHistory
{
public:
    static constexpr size_t maxSteps = 200;

    explicit EditHistory (DrumEngine& engineToWatch);

    // Takes the song as it is now as the starting point and forgets every step.
    void reset();

    // Records the song as one step if it differs from the last recorded state, and drops the redo steps.
    // Returns true when a step was recorded.
    bool commit();

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    // Each returns false, changing nothing, when there is nothing to undo or redo. Edits made since the
    // last commit are recorded first, so that an undo never loses them.
    bool undo();
    bool redo();

private:
    bool restore (const std::string& state);

    DrumEngine& engine;
    std::string current;
    std::deque<std::string> undoStack;
    std::deque<std::string> redoStack;
};
} // namespace drumbox
