#include "FlatLookAndFeel.h"

#include "Theme.h"

using namespace drumbox;

FlatLookAndFeel::FlatLookAndFeel()
{
    const auto t = makeTheme (SkinId::Dark);
    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::Label::textColourId, t.text);
    setColour (juce::Slider::textBoxTextColourId, t.text);
    setColour (juce::Slider::textBoxBackgroundColourId, t.cellOff);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::backgroundColourId, t.cellOff);
    setColour (juce::Slider::trackColourId, t.textDim);
    setColour (juce::Slider::thumbColourId, t.text);
    setColour (juce::TextButton::buttonColourId, t.cellOffAlt);
    setColour (juce::TextButton::buttonOnColourId, t.textDim);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TextButton::textColourOnId, t.background);
    setColour (juce::ComboBox::backgroundColourId, t.cellOffAlt);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::outlineColourId, t.panelEdge);
    setColour (juce::ComboBox::arrowColourId, t.textDim);
    setColour (juce::PopupMenu::backgroundColourId, t.panel);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::headerTextColourId, t.textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.panelEdge);
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);
    setColour (juce::TooltipWindow::backgroundColourId, t.panel);
    setColour (juce::TooltipWindow::textColourId, t.text);
    setColour (juce::TooltipWindow::outlineColourId, t.panelEdge);
    setColour (juce::AlertWindow::backgroundColourId, t.panel);
    setColour (juce::AlertWindow::textColourId, t.text);
    setColour (juce::AlertWindow::outlineColourId, t.panelEdge);
    setColour (juce::TextEditor::backgroundColourId, t.cellOff);
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::TextEditor::outlineColourId, t.panelEdge);
    setColour (juce::ScrollBar::thumbColourId, t.panelEdge);
}

void FlatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto& t = theme();

    juce::Path track, value;
    track.addCentredArc (area.getCentreX(), area.getCentreY(), diameter * 0.5f, diameter * 0.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    value.addCentredArc (area.getCentreX(), area.getCentreY(), diameter * 0.5f, diameter * 0.5f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (t.cellOffAlt);
    g.strokePath (track, juce::PathStrokeType (3.0f));
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (value, juce::PathStrokeType (3.0f));
}

void FlatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool highlighted, bool down)
{
    const auto& t = theme();
    auto area = button.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : button.findColour (juce::TextButton::buttonColourId);
    if (highlighted)
        fill = fill.brighter (0.12f);
    if (down)
        fill = fill.darker (0.15f);
    if (! button.isEnabled())
        fill = fill.withAlpha (0.4f);
    g.setColour (fill);
    g.fillRoundedRectangle (area, t.cornerSize());
}

juce::Font FlatLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jlimit (10.0f, 15.0f, (float) buttonHeight * 0.45f), juce::Font::bold));
}
