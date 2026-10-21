#include "PianoRoll.h"

#include <algorithm>

namespace drumbox::pianoRoll
{
namespace
{
int slotCount (RollTrack track) { return track == RollTrack::Pad ? synthVoices : track == RollTrack::Pluck ? pluckVoices : 1; }

int getSlot (const Pattern& p, RollTrack track, int step, int slot)
{
    return track == RollTrack::Bass ? p.getBassNote (step)
         : track == RollTrack::Pad  ? p.getSynthNote (step, slot)
                                    : p.getPluckNote (step, slot);
}

void setSlot (Pattern& p, RollTrack track, int step, int slot, int note)
{
    if (track == RollTrack::Bass)
        p.setBass (step, note, p.getBassAccent (step));
    else if (track == RollTrack::Pad)
        p.setSynthNote (step, slot, note);
    else
        p.setPluckNote (step, slot, note);
}
} // namespace

std::vector<int> notesAt (const Pattern& p, RollTrack track, int step)
{
    std::vector<int> notes;
    for (int slot = 0; slot < slotCount (track); ++slot)
        if (const int note = getSlot (p, track, step, slot); note != noNote)
            notes.push_back (note);
    std::sort (notes.begin(), notes.end());
    return notes;
}

bool toggleNote (Pattern& p, RollTrack track, int step, int note)
{
    note = std::clamp (note, lowestNote, highestNote);
    int freeSlot = -1;
    for (int slot = 0; slot < slotCount (track); ++slot)
    {
        const int held = getSlot (p, track, step, slot);
        if (held == note)
        {
            setSlot (p, track, step, slot, noNote);
            return true;
        }
        if (held == noNote && freeSlot < 0)
            freeSlot = slot;
    }
    if (track == RollTrack::Bass)
        freeSlot = 0;
    if (freeSlot < 0)
        return false;
    setSlot (p, track, step, freeSlot, note);
    return true;
}

void clearStep (Pattern& p, RollTrack track, int step)
{
    for (int slot = 0; slot < slotCount (track); ++slot)
        setSlot (p, track, step, slot, noNote);
}

bool isBlackKey (int note) noexcept
{
    const int pitchClass = note % 12;
    return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
}
} // namespace drumbox::pianoRoll
