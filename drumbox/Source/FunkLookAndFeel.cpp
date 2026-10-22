#include "FunkLookAndFeel.h"

using namespace drumbox;

FunkLookAndFeel::FunkLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, palette::cream);
    setColour (juce::Slider::textBoxBackgroundColourId, palette::brownInk);
    setColour (juce::Slider::textBoxOutlineColourId, palette::mustard);
    setColour (juce::Label::textColourId, palette::cream);
    setColour (juce::TextButton::textColourOffId, palette::brownInk);
    setColour (juce::TextButton::textColourOnId, palette::brownInk);
    setColour (juce::Slider::backgroundColourId, palette::brownInk);
    setColour (juce::Slider::trackColourId, palette::orange);
    setColour (juce::ComboBox::backgroundColourId, palette::mustard);
    setColour (juce::ComboBox::textColourId, palette::brownInk);
    setColour (juce::ComboBox::outlineColourId, palette::brownInk);
    setColour (juce::ComboBox::arrowColourId, palette::brownInk);
    setColour (juce::PopupMenu::backgroundColourId, palette::darkWood);
    setColour (juce::PopupMenu::textColourId, palette::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::orange);
    setColour (juce::PopupMenu::highlightedTextColourId, palette::brownInk);
    setColour (juce::TooltipWindow::backgroundColourId, palette::brownInk);
    setColour (juce::TooltipWindow::textColourId, palette::cream);
    setColour (juce::TooltipWindow::outlineColourId, palette::mustard);
}

void FunkLookAndFeel::paintWood (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setGradientFill (juce::ColourGradient (palette::lightWood, 0.0f, 0.0f, palette::wood,
                                              0.0f, (float) area.getHeight(), false));
    g.fillRect (area);

    // fixed seed: the grain must not change between repaints
    juce::Random random (70);
    for (int i = 0; i < 90; ++i)
    {
        const float y = random.nextFloat() * (float) area.getHeight();
        const float thickness = 0.5f + random.nextFloat() * 1.8f;
        const float bend = (random.nextFloat() - 0.5f) * 14.0f;
        juce::Path grain;
        grain.startNewSubPath ((float) area.getX(), y);
        grain.cubicTo ((float) area.getWidth() * 0.3f, y + bend, (float) area.getWidth() * 0.7f, y - bend,
                       (float) area.getRight(), y + bend * 0.5f);
        g.setColour (palette::darkWood.withAlpha (0.10f + random.nextFloat() * 0.18f));
        g.strokePath (grain, juce::PathStrokeType (thickness));
    }
}

void FunkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (palette::brownInk.withAlpha (0.6f));
    g.fillEllipse (knob.translated (3.0f, 4.0f));

    // tick ring
    g.setColour (palette::cream.withAlpha (0.7f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = rotaryStartAngle + (float) i / 10.0f * (rotaryEndAngle - rotaryStartAngle);
        const auto c = knob.getCentre();
        const float r1 = diameter * 0.5f + 1.0f, r2 = r1 + 4.0f;
        g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, 1.5f);
    }

    g.setGradientFill (juce::ColourGradient (palette::orange.brighter (0.3f), knob.getX(), knob.getY(),
                                              palette::orange.darker (0.5f), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (palette::brownInk);
    g.drawEllipse (knob, 3.0f);
    g.setColour (palette::mustard);
    g.drawEllipse (knob.reduced (7.0f), 2.0f);

    juce::Path pointer;
    const float length = diameter * 0.38f;
    pointer.addRoundedRectangle (-3.5f, -length, 7.0f, length * 0.7f, 2.0f);
    g.setColour (palette::cream);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (knob.getCentreX(), knob.getCentreY()));

    juce::ignoreUnused (slider);
}

void FunkLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool highlighted, bool down)
{
    auto area = button.getLocalBounds().toFloat().reduced (2.0f);
    auto base = button.getToggleState() ? palette::orange : palette::mustard;
    if (highlighted)
        base = base.brighter (0.15f);
    if (down)
        area = area.translated (0.0f, 2.0f);

    g.setColour (palette::brownInk.withAlpha (0.6f));
    g.fillRoundedRectangle (area.translated (0.0f, 3.0f), 10.0f);
    g.setColour (base);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (palette::brownInk);
    g.drawRoundedRectangle (area, 10.0f, 3.0f);
}

juce::Font FunkLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions ((float) buttonHeight * 0.5f, juce::Font::bold));
}

juce::Label* FunkLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    return label;
}
