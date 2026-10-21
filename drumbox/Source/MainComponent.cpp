#include "MainComponent.h"

#include "FunkLookAndFeel.h"

using namespace drumbox;

StepButton::StepButton (Pattern& patternToEdit, int trackIndex, int stepIndex)
    : juce::Button ({}), pattern (patternToEdit), track (trackIndex), step (stepIndex)
{
    setTitle (juce::String (trackNames[(size_t) trackIndex]) + " step " + juce::String (stepIndex + 1));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void StepButton::setCurrent (bool isCurrent)
{
    if (current != isCurrent)
    {
        current = isCurrent;
        repaint();
    }
}

void StepButton::clicked()
{
    pattern.cycle (track, step);
    repaint();
}

void StepButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto state = pattern.get (track, step);
    auto area = getLocalBounds().toFloat().reduced (2.5f);
    const bool beatGroup = (step / 4) % 2 == 0;

    juce::Colour fill = beatGroup ? palette::wood.darker (0.4f) : palette::wood.darker (0.1f);
    if (state == StepState::Normal)
        fill = palette::orange;
    else if (state == StepState::Accent)
        fill = palette::mustard;
    if (highlighted)
        fill = fill.brighter (0.12f);
    if (down)
        area = area.reduced (1.5f);

    g.setColour (palette::brownInk.withAlpha (0.55f));
    g.fillRoundedRectangle (area.translated (0.0f, 2.0f), 6.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (palette::brownInk);
    g.drawRoundedRectangle (area, 6.0f, 2.0f);

    // an accent also gets a ring so it does not rely on colour alone
    if (state == StepState::Accent)
    {
        g.setColour (palette::brownInk);
        g.drawRoundedRectangle (area.reduced (7.0f), 3.0f, 2.5f);
    }

    if (current)
    {
        g.setColour (palette::cream);
        g.drawRoundedRectangle (area.reduced (1.0f), 6.0f, 3.0f);
    }
}

MainComponent::MainComponent()
{
    juce::String error = deviceManager.initialiseWithDefaultDevices (0, 2);
    juce::ignoreUnused (error);
    deviceManager.addAudioCallback (this);

    titleLabel.setText ("DRUM BOX", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, palette::mustard);
    addAndMakeVisible (titleLabel);

    legendLabel.setText ("click a step: off / normal / ACCENT", juce::dontSendNotification);
    legendLabel.setColour (juce::Label::textColourId, palette::cream);
    legendLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (legendLabel);

    for (int t = 0; t < numTracks; ++t)
    {
        auto& label = trackLabels[(size_t) t];
        label.setText (trackNames[(size_t) t], juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, palette::cream);
        addAndMakeVisible (label);

        for (int s = 0; s < numSteps; ++s)
        {
            auto button = std::make_unique<StepButton> (engine.getPattern(), t, s);
            addAndMakeVisible (*button);
            steps[(size_t) (t * numSteps + s)] = std::move (button);
        }
    }

    playButton.setClickingTogglesState (true);
    playButton.setTitle ("Play / stop");
    playButton.onClick = [this]
    {
        engine.setPlaying (playButton.getToggleState());
        updateTransportButton();
    };
    addAndMakeVisible (playButton);

    auto setupKnob = [this] (juce::Slider& slider, juce::Label& label, const juce::String& text,
                             double min, double max, double initial, const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 24);
        slider.setRange (min, max, 1.0);
        slider.setValue (initial, juce::dontSendNotification);
        slider.setTextValueSuffix (suffix);
        slider.setTitle (text);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        addAndMakeVisible (slider);
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        addAndMakeVisible (label);
    };
    setupKnob (tempoSlider, tempoLabel, "TEMPO", Sequencer::minTempo, Sequencer::maxTempo, 110.0, " bpm");
    setupKnob (swingSlider, swingLabel, "SWING", 0.0, Sequencer::maxSwingPercent, 0.0, " %");
    tempoSlider.onValueChange = [this] { engine.setTempo (tempoSlider.getValue()); };
    swingSlider.onValueChange = [this] { engine.setSwing (swingSlider.getValue()); };
    engine.setTempo (tempoSlider.getValue());
    engine.setSwing (swingSlider.getValue());

    // a starter groove so the first press of PLAY is not silence
    auto& pattern = engine.getPattern();
    for (int s : { 0, 4, 8, 12 })
        pattern.set (Kick, s, StepState::Normal);
    pattern.set (Kick, 0, StepState::Accent);
    for (int s : { 4, 12 })
        pattern.set (Snare, s, StepState::Accent);
    for (int s = 0; s < numSteps; s += 2)
        pattern.set (ClosedHat, s, StepState::Normal);
    pattern.set (OpenHat, 14, StepState::Normal);

    updateTransportButton();
    setSize (1000, 560);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeAudioCallback (this);
}

void MainComponent::updateTransportButton()
{
    playButton.setButtonText (playButton.getToggleState() ? "STOP" : "PLAY");
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    engine.prepare (device->getCurrentSampleRate());
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* output,
                                                      int numOutputChannels, int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    if (numOutputChannels <= 0 || output[0] == nullptr)
        return;

    engine.render (output[0], numSamples);
    for (int ch = 1; ch < numOutputChannels; ++ch)
        if (output[ch] != nullptr)
            juce::FloatVectorOperations::copy (output[ch], output[0], numSamples);
}

void MainComponent::timerCallback()
{
    const int current = engine.getCurrentStep();
    if (current == shownStep)
        return;

    for (int t = 0; t < numTracks; ++t)
    {
        if (shownStep >= 0)
            steps[(size_t) (t * numSteps + shownStep)]->setCurrent (false);
        if (current >= 0)
            steps[(size_t) (t * numSteps + current)]->setCurrent (true);
    }
    shownStep = current;
}

void MainComponent::paint (juce::Graphics& g)
{
    FunkLookAndFeel::paintWood (g, getLocalBounds());

    // inset dark panel behind the grid
    auto grid = getLocalBounds().reduced (16).withTrimmedTop (70).withTrimmedRight (150);
    g.setColour (palette::brownInk.withAlpha (0.55f));
    g.fillRoundedRectangle (grid.toFloat(), 12.0f);
    g.setColour (palette::mustard);
    g.drawRoundedRectangle (grid.toFloat(), 12.0f, 3.0f);

    // racing stripe under the title, a 70's staple
    const int y = 58;
    g.setColour (palette::orange);
    g.fillRect (16, y, getWidth() - 32, 3);
    g.setColour (palette::mustard);
    g.fillRect (16, y + 5, getWidth() - 32, 3);
    g.setColour (palette::avocado);
    g.fillRect (16, y + 10, getWidth() - 32, 3);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto header = area.removeFromTop (54);
    titleLabel.setBounds (header.removeFromLeft (300));
    legendLabel.setBounds (header);
    area.removeFromTop (16);

    auto side = area.removeFromRight (134);
    auto grid = area.reduced (10);
    const int labelWidth = 80;
    const int rowHeight = grid.getHeight() / numTracks;
    const int cellWidth = (grid.getWidth() - labelWidth) / numSteps;

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = grid.getY() + t * rowHeight;
        trackLabels[(size_t) t].setBounds (grid.getX(), y, labelWidth, rowHeight);
        for (int s = 0; s < numSteps; ++s)
            steps[(size_t) (t * numSteps + s)]->setBounds (grid.getX() + labelWidth + s * cellWidth, y, cellWidth, rowHeight);
    }

    side.removeFromLeft (14);
    playButton.setBounds (side.removeFromTop (64));
    side.removeFromTop (14);
    const int knobBlock = (side.getHeight() - 14) / 2;
    auto tempoArea = side.removeFromTop (knobBlock);
    tempoLabel.setBounds (tempoArea.removeFromTop (20));
    tempoSlider.setBounds (tempoArea);
    side.removeFromTop (14);
    swingLabel.setBounds (side.removeFromTop (20));
    swingSlider.setBounds (side);
}
