#pragma once

#include "Mixer.h"
#include "Pattern.h"
#include "Settings.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace drumbox
{
// Everything the custom-painted parts of the window read, so that a skin is a swap of colours and shapes
// and the layout stays the same.
struct Theme
{
    bool flat = true;
    juce::Colour background, panel, panelEdge, text, textDim, cellOff, cellOffAlt, cursor, danger;
    std::array<juce::Colour, numMixerTracks> track;

    juce::Colour trackColour (int mixerTrack) const;
    // The fill of a step: the track colour, a stronger one for an accent.
    juce::Colour stepColour (int mixerTrack, StepState state) const;
    float cornerSize() const noexcept { return flat ? 4.0f : 6.0f; }
};

// The theme of a skin, whichever is active.
Theme makeTheme (SkinId skin);
const Theme& theme();
SkinId currentSkin();
void setSkin (SkinId skin);
const char* skinName (SkinId skin);

// Paints the window background of the current skin.
void paintBackground (juce::Graphics&, juce::Rectangle<int> area);
} // namespace drumbox
