#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace drumbox::palette
{
const juce::Colour darkWood { 0xff2e1a0e };
const juce::Colour wood { 0xff5a3319 };
const juce::Colour lightWood { 0xff7d4a24 };
const juce::Colour orange { 0xffe8731a };
const juce::Colour mustard { 0xffe6b02f };
const juce::Colour avocado { 0xff8a9a2b };
const juce::Colour teal { 0xff3f9a94 };
const juce::Colour cream { 0xfff4e4bd };
const juce::Colour brownInk { 0xff1f1109 };
} // namespace drumbox::palette

// Chunky Bakelite-style knobs, cream-on-brown text, mustard transport button.
class FunkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FunkLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Warm wood-grain panel used as the window background.
    static void paintWood (juce::Graphics&, juce::Rectangle<int> area);
};
