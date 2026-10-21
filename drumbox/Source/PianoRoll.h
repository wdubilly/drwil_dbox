#pragma once

#include "Pattern.h"

#include <vector>

namespace drumbox
{
enum class RollTrack
{
    Bass,
    Pad,
    Pluck
};

// Note entry of the piano roll. It writes the same pattern data as the step-by-step entry
// (setBass, setSynthNote, setPluckNote): there is no second storage.
namespace pianoRoll
{
// The notes sounding on the step, lowest first.
std::vector<int> notesAt (const Pattern&, RollTrack, int step);

// Puts the note on the step, or takes it off if it is already there. The bass has one note per
// step, so a new note replaces the old one; the pad and the pluck take it in a free slot.
// Returns false, changing nothing, when the pad or the pluck step already holds four other notes.
bool toggleNote (Pattern&, RollTrack, int step, int note);

// Empties the step; its accent is kept.
void clearStep (Pattern&, RollTrack, int step);

bool isBlackKey (int note) noexcept;
} // namespace pianoRoll
} // namespace drumbox
