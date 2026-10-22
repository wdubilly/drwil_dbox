#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The default skin: dark, flat, thin lines, one accent colour per track (see Theme).
class FlatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
