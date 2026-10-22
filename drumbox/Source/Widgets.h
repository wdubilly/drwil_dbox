#pragma once

#include "DrumEngine.h"
#include "PianoRoll.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

juce::String noteName (int note);

// A horizontal bar showing the name and the value of a setting; drag to change, double-click to reset.
class ParamSlider : public juce::Slider
{
public:
    // The bar takes the colour of the mixer track when one is given.
    ParamSlider (juce::String settingName, int mixerTrack = -1);

    void setMixerTrack (int mixerTrack);
    void setName (const juce::String& settingName) { label = settingName; repaint(); }
    void paint (juce::Graphics&) override;
    void lookAndFeelChanged() override;

private:
    juce::String label;
    int track;
};

// Piano roll of the bass, the pad or the pluck: the 16 steps across, the pitches up. It writes the same
// pattern data as the step-by-step rows. Click: note on / off. Right-click: accent of the step.
class PianoRollView : public juce::Component, public juce::SettableTooltipClient
{
public:
    PianoRollView (drumbox::Song& songToEdit, const int& editedPattern, drumbox::RollTrack trackToEdit);

    void setCurrentStep (int step);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int rows = 25;
    static constexpr int keyWidth = 52;
    juce::Rectangle<int> grid() const;
    int mixerTrack() const;
    void shiftOctave (int octaves);
    bool accentOf (int step) const;
    void toggleAccent (int step);

    drumbox::Song& song;
    const int& editing;
    drumbox::RollTrack track;
    int lowest;
    int current = -1;
    juce::TextButton lower { "OCT -" }, higher { "OCT +" };
};

// The six rows of the FX track: click a step to cycle off / normal / accent.
class FxRowsView : public juce::Component, public juce::SettableTooltipClient
{
public:
    FxRowsView (drumbox::Song& songToEdit, const int& editedPattern);

    void setCurrentStep (int step);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    static constexpr int labelWidth = 70;
    juce::Rectangle<int> grid() const;

    drumbox::Song& song;
    const int& editing;
    int current = -1;
};

// A pattern number of the bank: click to edit it, drag it onto a position of the chain to place it there.
class PatternButton : public juce::TextButton
{
public:
    explicit PatternButton (int patternIndex);

    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int pattern;
    bool dragged = false;
};

// One position of the chain: shows the pattern played there. Click: give it the edited pattern. Drop a
// pattern number on it to place that pattern; drag it onto another position to swap the two.
class ChainCell : public juce::Button, public juce::DragAndDropTarget
{
public:
    ChainCell (drumbox::Song& songToEdit, const int& editedPattern, int positionIndex);

    void setCurrent (bool isCurrent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    drumbox::Song& song;
    const int& editing;
    int position;
    bool current = false;
    bool dropTarget = false;
    bool dragged = false;
};

// The chain as a strip under the grid: one cell per position, as long as the chain.
class ChainStrip : public juce::Component
{
public:
    static constexpr int cellWidth = 46;

    ChainStrip (drumbox::Song& songToEdit, const int& editedPattern);

    // Shows as many cells as the chain has positions; call when the length changes.
    void refresh (int height);
    void setCurrentPosition (int position);
    int cellX (int position) const noexcept { return position * cellWidth; }

private:
    drumbox::Song& song;
    std::array<std::unique_ptr<ChainCell>, drumbox::maxChainLength> cells;
    int shown = -1;
    int length = 0;
};
