#include "SequenceView.h"

#include "FunkLookAndFeel.h"

using namespace drumbox;

SequenceRow::SequenceRow (DrumEngine& engineToShow, const int& editedPattern, int mixerTrack,
                          std::function<void (int)> onSelect, std::function<void()> onAudibilityChange)
    : engine (engineToShow), editing (editedPattern), track (mixerTrack), select (std::move (onSelect)),
      audibilityChanged (std::move (onAudibilityChange)), volume ("VOL", mixerTrack)
{
    const juce::String name = mixerTrackNames[(size_t) track];
    setTitle (name + " track");

    muteButton.setTooltip ("Mute " + name);
    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (engine.getMixer().isMuted (track), juce::dontSendNotification);
    muteButton.onClick = [this]
    {
        engine.getMixer().setMute (track, muteButton.getToggleState());
        audibilityChanged();
    };
    addAndMakeVisible (muteButton);

    soloButton.setTooltip ("Solo " + name + ": only the soloed tracks are heard");
    soloButton.setClickingTogglesState (true);
    soloButton.setToggleState (engine.getMixer().isSolo (track), juce::dontSendNotification);
    soloButton.onClick = [this]
    {
        engine.getMixer().setSolo (track, soloButton.getToggleState());
        audibilityChanged();
    };
    addAndMakeVisible (soloButton);

    volume.setRange (0.0, maxVolume * 100.0, 1.0);
    volume.setTextValueSuffix (" %");
    volume.setDoubleClickReturnValue (true, 100.0);
    volume.setTooltip (name + " volume. Double-click to reset.");
    volume.setValue (engine.getMixer().getVolume (track) * 100.0, juce::dontSendNotification);
    volume.onValueChange = [this] { engine.getMixer().setVolume (track, (float) volume.getValue() / 100.0f); };
    addAndMakeVisible (volume);
    lookAndFeelChanged();
}

juce::Rectangle<int> SequenceRow::cellsArea (juce::Rectangle<int> rowBounds)
{
    return rowBounds.withTrimmedLeft (nameWidth + 2 * buttonWidth + volumeWidth + 14).withTrimmedRight (6);
}

void SequenceRow::setSelected (bool isSelected)
{
    if (selected != isSelected)
    {
        selected = isSelected;
        repaint();
    }
}

void SequenceRow::refreshFromMixer()
{
    const auto& mixer = engine.getMixer();
    volume.setValue (mixer.getVolume (track) * 100.0, juce::dontSendNotification);
    muteButton.setToggleState (mixer.isMuted (track), juce::dontSendNotification);
    soloButton.setToggleState (mixer.isSolo (track), juce::dontSendNotification);
    repaint();
}

void SequenceRow::lookAndFeelChanged()
{
    const auto& t = theme();
    muteButton.setColour (juce::TextButton::buttonOnColourId, t.flat ? t.danger : palette::orange);
    soloButton.setColour (juce::TextButton::buttonOnColourId, t.flat ? t.trackColour (2) : palette::mustard);
    volume.setMixerTrack (track);
}

StepState SequenceRow::stateAt (int step) const
{
    const auto& p = engine.getSong().pattern (editing);
    if (track < numTracks)
        return p.get (track, step);
    if (track == mixerBass)
        return p.getBassNote (step) != noNote ? (p.getBassAccent (step) ? StepState::Accent : StepState::Normal) : StepState::Off;
    if (track == mixerFx)
    {
        StepState best = StepState::Off;
        for (int r = 0; r < numFxRows; ++r)
            if (static_cast<int> (p.getFx (r, step)) > static_cast<int> (best))
                best = p.getFx (r, step);
        return best;
    }
    const bool pad = track == mixerPad;
    bool any = false;
    for (int v = 0; v < (pad ? synthVoices : pluckVoices); ++v)
        any = any || (pad ? p.getSynthNote (step, v) : p.getPluckNote (step, v)) != noNote;
    if (! any)
        return StepState::Off;
    return (pad ? p.getSynthAccent (step) : p.getPluckAccent (step)) ? StepState::Accent : StepState::Normal;
}

int SequenceRow::noteAt (int step) const
{
    return track == mixerBass ? engine.getSong().pattern (editing).getBassNote (step) : noNote;
}

void SequenceRow::resized()
{
    auto area = getLocalBounds().reduced (4, 3);
    area.removeFromLeft (nameWidth);
    muteButton.setBounds (area.removeFromLeft (buttonWidth).reduced (2, 0));
    soloButton.setBounds (area.removeFromLeft (buttonWidth).reduced (2, 0));
    area.removeFromLeft (4);
    volume.setBounds (area.removeFromLeft (volumeWidth).reduced (0, 1));
}

void SequenceRow::mouseDown (const juce::MouseEvent& e)
{
    select (track);
    const auto cells = cellsArea (getLocalBounds());
    if (track < numTracks && cells.contains (e.getPosition()))
    {
        const int step = juce::jlimit (0, numSteps - 1, (e.x - cells.getX()) * numSteps / cells.getWidth());
        engine.getSong().pattern (editing).cycle (track, step);
        repaint();
    }
}

void SequenceRow::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto colour = t.trackColour (track);
    const bool heard = engine.getMixer().targetGain (track, engine.getMixer().anySolo()) > 0.0f;
    auto bounds = getLocalBounds().toFloat().reduced (1.0f, 1.0f);

    if (selected)
    {
        g.setColour (t.flat ? t.panelEdge.withAlpha (0.55f) : palette::mustard.withAlpha (0.25f));
        g.fillRoundedRectangle (bounds, t.cornerSize());
        g.setColour (colour);
        g.fillRoundedRectangle (bounds.removeFromLeft (4.0f), 2.0f);
    }
    else
    {
        g.setColour (colour.withAlpha (heard ? 1.0f : 0.35f));
        g.fillRoundedRectangle (bounds.removeFromLeft (4.0f), 2.0f);
    }

    g.setColour (heard ? t.text : t.textDim.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (11.0f, 15.0f, (float) getHeight() * 0.5f), juce::Font::bold)));
    g.drawText (mixerTrackNames[(size_t) track], 12, 0, nameWidth - 12, getHeight(), juce::Justification::centredLeft);

    const auto cells = cellsArea (getLocalBounds());
    const float cellW = (float) cells.getWidth() / numSteps;
    for (int s = 0; s < numSteps; ++s)
    {
        const auto state = stateAt (s);
        auto cell = juce::Rectangle<float> ((float) cells.getX() + (float) s * cellW, (float) cells.getY(), cellW, (float) cells.getHeight()).reduced (1.5f, 2.0f);
        auto fill = state == StepState::Off ? ((s / 4) % 2 == 0 ? t.cellOff : t.cellOffAlt) : t.stepColour (track, state);
        if (! heard)
            fill = fill.withAlpha (0.45f);
        if (! t.flat)
        {
            g.setColour (palette::brownInk.withAlpha (0.55f));
            g.fillRoundedRectangle (cell.translated (0.0f, 1.5f), t.cornerSize());
        }
        g.setColour (fill);
        g.fillRoundedRectangle (cell, t.cornerSize());
        if (! t.flat)
        {
            g.setColour (palette::brownInk);
            g.drawRoundedRectangle (cell, t.cornerSize(), 1.5f);
        }
        if (state == StepState::Accent)
        {
            g.setColour (t.flat ? t.background : palette::brownInk);
            g.drawRoundedRectangle (cell.reduced (juce::jmin (cell.getHeight(), cell.getWidth()) * 0.28f), 2.0f, 1.5f);
        }
        if (const int note = noteAt (s); note != noNote && cell.getHeight() >= 14.0f)
        {
            g.setColour (t.flat ? t.background : palette::brownInk);
            g.setFont (juce::Font (juce::FontOptions (juce::jmin (12.0f, cell.getHeight() * 0.6f), juce::Font::bold)));
            g.drawText (noteName (note), cell.toNearestInt(), juce::Justification::centred);
        }
    }
}

SequenceView::SequenceView (DrumEngine& engineToShow, const int& editedPattern, std::function<void (int)> onSelect)
    : engine (engineToShow)
{
    for (int t = 0; t < numMixerTracks; ++t)
    {
        rows[(size_t) t] = std::make_unique<SequenceRow> (engine, editedPattern, t, onSelect,
                                                          [this]
                                                          {
                                                              for (auto& r : rows)
                                                                  r->repaint();
                                                          });
        addAndMakeVisible (*rows[(size_t) t]);
    }
    setTooltip ("The twelve tracks. Click a drum step: off, normal, accent. Click a track to edit its sound below.");
}

void SequenceView::setSelectedTrack (int mixerTrack)
{
    for (int t = 0; t < numMixerTracks; ++t)
        rows[(size_t) t]->setSelected (t == mixerTrack);
}

void SequenceView::setCurrentStep (int step)
{
    if (currentStep == step)
        return;
    currentStep = step;
    repaint();
}

void SequenceView::refreshFromMixer()
{
    for (auto& r : rows)
        r->refreshFromMixer();
}

void SequenceView::resized()
{
    auto area = getLocalBounds().withTrimmedTop (rulerHeight);
    const int rowHeight = area.getHeight() / numMixerTracks;
    for (int t = 0; t < numMixerTracks; ++t)
        rows[(size_t) t]->setBounds (area.getX(), area.getY() + t * rowHeight, area.getWidth(), rowHeight);
}

void SequenceView::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto cells = SequenceRow::cellsArea (getLocalBounds());
    const float cellW = (float) cells.getWidth() / numSteps;
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    for (int s = 0; s < numSteps; ++s)
    {
        g.setColour (s % 4 == 0 ? t.text : t.textDim.withAlpha (0.7f));
        g.drawText (juce::String (s + 1), juce::Rectangle<float> ((float) cells.getX() + (float) s * cellW, 0.0f, cellW, (float) rulerHeight).toNearestInt(),
                    juce::Justification::centred);
    }
}

void SequenceView::paintOverChildren (juce::Graphics& g)
{
    if (currentStep < 0)
        return;
    const auto& t = theme();
    const auto cells = SequenceRow::cellsArea (getLocalBounds());
    const float cellW = (float) cells.getWidth() / numSteps;
    const auto column = juce::Rectangle<float> ((float) cells.getX() + (float) currentStep * cellW, 0.0f, cellW, (float) getHeight());
    g.setColour (t.cursor.withAlpha (0.14f));
    g.fillRect (column);
    g.setColour (t.cursor.withAlpha (0.85f));
    g.fillRect (column.withWidth (2.0f).translated (0.0f, 0.0f));
    g.fillRect (juce::Rectangle<float> (column.getX(), 0.0f, cellW, 3.0f));
}
