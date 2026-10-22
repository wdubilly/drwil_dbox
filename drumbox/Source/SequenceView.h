#pragma once

#include "DrumEngine.h"
#include "Widgets.h"

#include <array>
#include <functional>
#include <memory>

// One line of the Sequence view: the track's name, mute, solo, volume and its 16 steps. A click on a drum
// step cycles off / normal / accent; a click anywhere else on the line selects the track for the detail panel.
class SequenceRow : public juce::Component
{
public:
    SequenceRow (drumbox::DrumEngine& engineToShow, const int& editedPattern, int mixerTrack,
                 std::function<void (int)> onSelect, std::function<void()> onAudibilityChange);

    static juce::Rectangle<int> cellsArea (juce::Rectangle<int> rowBounds);

    void setSelected (bool isSelected);
    // Shows again the mixer values of the track, after a preset or an undo changed them.
    void refreshFromMixer();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void lookAndFeelChanged() override;

private:
    static constexpr int nameWidth = 92;
    static constexpr int buttonWidth = 30;
    static constexpr int volumeWidth = 104;

    drumbox::StepState stateAt (int step) const;
    bool hasAccent (int step) const;
    int noteAt (int step) const;

    drumbox::DrumEngine& engine;
    const int& editing;
    int track;
    std::function<void (int)> select;
    std::function<void()> audibilityChanged;
    bool selected = false;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    ParamSlider volume;
};

// The twelve tracks stacked, a step ruler above and a playhead over the steps while playing.
class SequenceView : public juce::Component, public juce::SettableTooltipClient
{
public:
    SequenceView (drumbox::DrumEngine& engineToShow, const int& editedPattern, std::function<void (int)> onSelect);

    void setSelectedTrack (int mixerTrack);
    void setCurrentStep (int step);
    void refreshFromMixer();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static constexpr int rulerHeight = 18;

private:
    drumbox::DrumEngine& engine;
    std::array<std::unique_ptr<SequenceRow>, drumbox::numMixerTracks> rows;
    int currentStep = -1;
};
