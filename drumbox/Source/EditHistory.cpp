#include "EditHistory.h"

#include "SongFile.h"

namespace drumbox
{
EditHistory::EditHistory (DrumEngine& engineToWatch) : engine (engineToWatch), current (serializeSong (engineToWatch)) {}

void EditHistory::reset()
{
    undoStack.clear();
    redoStack.clear();
    current = serializeSong (engine);
}

bool EditHistory::commit()
{
    auto now = serializeSong (engine);
    if (now == current)
        return false;
    undoStack.push_back (std::move (current));
    if (undoStack.size() > maxSteps)
        undoStack.pop_front();
    redoStack.clear();
    current = std::move (now);
    return true;
}

bool EditHistory::restore (const std::string& state)
{
    return parseSong (state, engine);
}

bool EditHistory::undo()
{
    commit();
    if (undoStack.empty())
        return false;
    if (! restore (undoStack.back()))
        return false;
    redoStack.push_back (std::move (current));
    undoStack.pop_back();
    current = serializeSong (engine);
    return true;
}

bool EditHistory::redo()
{
    commit();
    if (redoStack.empty())
        return false;
    if (! restore (redoStack.back()))
        return false;
    undoStack.push_back (std::move (current));
    redoStack.pop_back();
    current = serializeSong (engine);
    return true;
}
} // namespace drumbox
