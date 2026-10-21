#include "MainComponent.h"

#include "FunkLookAndFeel.h"

using namespace drumbox;

StepButton::StepButton (Song& songToEdit, const int& editedPattern, int trackIndex, int stepIndex, bool isFxRow)
    : juce::Button ({}), song (songToEdit), editing (editedPattern), track (trackIndex), step (stepIndex), fx (isFxRow)
{
    setTitle (juce::String (fx ? fxNames[(size_t) trackIndex] : trackNames[(size_t) trackIndex]) + " step " + juce::String (stepIndex + 1));
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
    if (fx)
        pattern().cycleFx (track, step);
    else
        pattern().cycle (track, step);
    repaint();
}

void StepButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto state = fx ? pattern().getFx (track, step) : pattern().get (track, step);
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

NoteButton::NoteButton (Song& songToEdit, const int& editedPattern, Kind kindOfNote, int slotIndex, int stepIndex)
    : juce::Button ({}), song (songToEdit), editing (editedPattern), kind (kindOfNote), slot (slotIndex), step (stepIndex)
{
    static constexpr int synthDefaults[synthVoices] = { 60, 64, 67, 71 };
    lastNote = kind == Kind::Bass ? 36 : synthDefaults[slot];
    const juce::String name = kind == Kind::Bass ? "BASS" : (kind == Kind::Pad ? "PAD note " : "PLUCK note ") + juce::String (slot + 1);
    setTitle (name + " step " + juce::String (stepIndex + 1));
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

juce::String NoteButton::noteName (int note)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}

int NoteButton::getNote() const
{
    if (kind == Kind::Bass)
        return pattern().getBassNote (step);
    return kind == Kind::Pad ? pattern().getSynthNote (step, slot) : pattern().getPluckNote (step, slot);
}

void NoteButton::setNote (int note)
{
    if (note != noNote)
        lastNote = note;
    if (kind == Kind::Bass)
        pattern().setBass (step, note, pattern().getBassAccent (step));
    else if (kind == Kind::Pad)
        pattern().setSynthNote (step, slot, note);
    else
        pattern().setPluckNote (step, slot, note);
}

bool NoteButton::getAccent() const
{
    if (kind == Kind::Bass)
        return pattern().getBassAccent (step);
    return kind == Kind::Pad ? pattern().getSynthAccent (step) : pattern().getPluckAccent (step);
}

void NoteButton::toggleAccent()
{
    if (kind == Kind::Bass)
        pattern().setBass (step, pattern().getBassNote (step), ! pattern().getBassAccent (step));
    else if (kind == Kind::Pad)
        pattern().setSynthAccent (step, ! pattern().getSynthAccent (step));
    else
        pattern().setPluckAccent (step, ! pattern().getPluckAccent (step));
}

void NoteButton::setCurrent (bool isCurrent)
{
    if (current != isCurrent)
    {
        current = isCurrent;
        repaint();
    }
}

void NoteButton::clicked()
{
    if (dragged)
        return;
    if (getNote() == noNote)
        setNote (lastNote);
    else
        setNote (noNote);
    repaint();
}

void NoteButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        toggleAccent();
        if (auto* parent = getParentComponent())
            parent->repaint();
        return;
    }
    dragStartNote = getNote();
    dragStartY = e.y;
    dragged = false;
    Button::mouseDown (e);
}

void NoteButton::mouseDrag (const juce::MouseEvent& e)
{
    Button::mouseDrag (e);
    if (dragStartNote == noNote)
        return;
    const int delta = (dragStartY - e.y) / 7;
    if (delta != 0 || dragged)
    {
        dragged = true;
        setNote (juce::jlimit (lowestNote, highestNote, dragStartNote + delta));
        repaint();
    }
}

void NoteButton::mouseUp (const juce::MouseEvent& e)
{
    Button::mouseUp (e);
    dragged = false;
}

void NoteButton::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int note = getNote();
    if (note == noNote || wheel.deltaY == 0.0f)
        return;
    setNote (juce::jlimit (lowestNote, highestNote, note + (wheel.deltaY > 0.0f ? 1 : -1)));
    repaint();
}

void NoteButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const int note = getNote();
    const bool accent = getAccent();
    auto area = getLocalBounds().toFloat().reduced (2.5f);
    const bool beatGroup = (step / 4) % 2 == 0;

    juce::Colour fill = beatGroup ? palette::wood.darker (0.4f) : palette::wood.darker (0.1f);
    if (note != noNote)
        fill = accent ? palette::mustard : (kind == Kind::Bass ? palette::orange : kind == Kind::Pad ? palette::avocado.brighter (0.2f) : palette::teal);
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

    if (note != noNote)
    {
        g.setFont (juce::Font (juce::FontOptions (juce::jmin (16.0f, area.getHeight() * 0.45f), juce::Font::bold)));
        g.drawText (noteName (note), area.toNearestInt(), juce::Justification::centred);
        // an accent also gets a ring so it does not rely on colour alone
        if (accent)
            g.drawRoundedRectangle (area.reduced (5.0f), 3.0f, 2.5f);
    }

    if (current)
    {
        g.setColour (palette::cream);
        g.drawRoundedRectangle (area.reduced (1.0f), 6.0f, 3.0f);
    }
}

ChainCell::ChainCell (Song& songToEdit, const int& editedPattern, int positionIndex)
    : juce::Button ({}), song (songToEdit), editing (editedPattern), position (positionIndex)
{
    setTitle ("Chain position " + juce::String (positionIndex + 1));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChainCell::setCurrent (bool isCurrent)
{
    if (current != isCurrent)
    {
        current = isCurrent;
        repaint();
    }
}

void ChainCell::clicked()
{
    song.setChainEntry (position, editing);
    repaint();
}

void ChainCell::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const bool inChain = position < song.getChainLength();
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    if (down)
        area = area.reduced (1.5f);

    juce::Colour fill = inChain ? palette::orange : palette::wood.darker (0.3f);
    if (highlighted)
        fill = fill.brighter (0.12f);

    g.setColour (palette::brownInk.withAlpha (0.55f));
    g.fillRoundedRectangle (area.translated (0.0f, 2.0f), 5.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (palette::brownInk);
    g.drawRoundedRectangle (area, 5.0f, 2.0f);

    g.setColour (inChain ? palette::brownInk : palette::cream.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (juce::jmin (20.0f, area.getHeight() * 0.5f), juce::Font::bold)));
    g.drawText (juce::String (song.getChainEntry (position) + 1), area.toNearestInt(), juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (juce::String (position + 1), area.reduced (4.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);

    if (current)
    {
        g.setColour (palette::cream);
        g.drawRoundedRectangle (area.reduced (1.0f), 5.0f, 3.0f);
    }
}

namespace
{
constexpr int nameWidth = 86;
constexpr int buttonWidth = 34;
constexpr int controlsWidth = 2 * buttonWidth + 4 * 150;
} // namespace

TrackStrip::TrackStrip (Song& songToShow, Mixer& mixerToEdit, int trackIndex, std::function<void (int)> onOpen)
    : song (songToShow), mixer (mixerToEdit), track (trackIndex), open (std::move (onOpen))
{
    const juce::String name = mixerTrackNames[(size_t) trackIndex];

    nameButton.setButtonText (name);
    nameButton.setTitle ("Open the " + name + " editor");
    nameButton.onClick = [this] { open (track); };
    addAndMakeVisible (nameButton);

    muteButton.setTitle ("Mute " + name);
    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonOnColourId, palette::orange);
    muteButton.setToggleState (mixer.isMuted (track), juce::dontSendNotification);
    muteButton.onClick = [this]
    {
        mixer.setMute (track, muteButton.getToggleState());
        if (onAudibilityChange)
            onAudibilityChange();
    };
    addAndMakeVisible (muteButton);

    soloButton.setTitle ("Solo " + name);
    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, palette::mustard);
    soloButton.setToggleState (mixer.isSolo (track), juce::dontSendNotification);
    soloButton.onClick = [this]
    {
        mixer.setSolo (track, soloButton.getToggleState());
        if (onAudibilityChange)
            onAudibilityChange();
    };
    addAndMakeVisible (soloButton);

    const auto setup = [this, &name] (juce::Slider& slider, const juce::String& what, double min, double max, double step,
                                      double initial, const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 22);
        slider.setRange (min, max, step);
        slider.setValue (initial, juce::dontSendNotification);
        slider.setTextValueSuffix (suffix);
        slider.setDoubleClickReturnValue (true, what == "volume" ? 100.0 : 0.0);
        slider.setTitle (name + " " + what);
        addAndMakeVisible (slider);
    };
    setup (volume, "volume", 0.0, maxVolume * 100.0, 1.0, mixer.getVolume (track) * 100.0, " %");
    setup (tune, "tune", -maxTuneSemitones, maxTuneSemitones, 1.0, mixer.getTune (track), " st");
    setup (detune, "detune", -maxDetuneCents, maxDetuneCents, 1.0, mixer.getDetune (track), " ct");
    setup (send, "send", 0.0, maxSend * 100.0, 1.0, mixer.getSend (track) * 100.0, " %");
    send.onValueChange = [this] { mixer.setSend (track, (float) send.getValue() / 100.0f); };
    volume.onValueChange = [this] { mixer.setVolume (track, (float) volume.getValue() / 100.0f); };
    tune.onValueChange = [this] { mixer.setTune (track, (float) tune.getValue()); };
    detune.onValueChange = [this] { mixer.setDetune (track, (float) detune.getValue()); };
}

void TrackStrip::refreshFromMixer()
{
    volume.setValue (mixer.getVolume (track) * 100.0, juce::dontSendNotification);
    tune.setValue (mixer.getTune (track), juce::dontSendNotification);
    detune.setValue (mixer.getDetune (track), juce::dontSendNotification);
    send.setValue (mixer.getSend (track) * 100.0, juce::dontSendNotification);
    muteButton.setToggleState (mixer.isMuted (track), juce::dontSendNotification);
    soloButton.setToggleState (mixer.isSolo (track), juce::dontSendNotification);
}

void TrackStrip::setCurrentPosition (int position)
{
    if (currentPosition != position)
    {
        currentPosition = position;
        repaint (mapArea());
    }
}

bool TrackStrip::playsIn (const Pattern& pattern) const
{
    for (int s = 0; s < numSteps; ++s)
    {
        if (track < numTracks)
        {
            if (pattern.get (track, s) != StepState::Off)
                return true;
        }
        else if (track == mixerBass)
        {
            if (pattern.getBassNote (s) != noNote)
                return true;
        }
        else if (track == mixerFx)
        {
            for (int r = 0; r < numFxRows; ++r)
                if (pattern.getFx (r, s) != StepState::Off)
                    return true;
        }
        else
        {
            const int voices = track == mixerPad ? synthVoices : pluckVoices;
            for (int v = 0; v < voices; ++v)
                if ((track == mixerPad ? pattern.getSynthNote (s, v) : pattern.getPluckNote (s, v)) != noNote)
                    return true;
        }
    }
    return false;
}

juce::Rectangle<int> TrackStrip::mapArea() const
{
    return getLocalBounds().reduced (4, 4).withTrimmedLeft (nameWidth + 6).withTrimmedRight (controlsWidth + 6).reduced (0, 4);
}

void TrackStrip::mouseUp (const juce::MouseEvent& e)
{
    if (mapArea().contains (e.getPosition()) && e.mouseWasClicked())
        open (track);
}

void TrackStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const bool heard = mixer.targetGain (track, mixer.anySolo()) > 0.0f;
    g.setColour (palette::brownInk.withAlpha (0.45f));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (heard ? palette::mustard.withAlpha (0.7f) : palette::wood);
    g.drawRoundedRectangle (area, 6.0f, 1.5f);

    // the chain, one cell per position: lit where this track plays in that position's pattern
    const auto map = mapArea();
    const int length = song.getChainLength();
    const float cellWidth = (float) map.getWidth() / (float) length;
    for (int p = 0; p < length; ++p)
    {
        auto cell = juce::Rectangle<float> ((float) map.getX() + (float) p * cellWidth, (float) map.getY(), cellWidth, (float) map.getHeight())
                        .reduced (cellWidth > 6.0f ? 1.0f : 0.0f, 0.0f);
        g.setColour (playsIn (song.pattern (song.getChainEntry (p))) ? palette::orange : palette::wood.darker (0.3f));
        g.fillRect (cell);
        if (p == currentPosition)
        {
            g.setColour (palette::cream);
            g.drawRect (cell, 2.0f);
        }
    }
    g.setColour (palette::brownInk);
    g.drawRect (map.toFloat(), 1.0f);
}

void TrackStrip::resized()
{
    auto area = getLocalBounds().reduced (4, 4);
    nameButton.setBounds (area.removeFromLeft (nameWidth));
    auto controls = area.removeFromRight (controlsWidth);
    muteButton.setBounds (controls.removeFromLeft (buttonWidth).reduced (2, 0));
    soloButton.setBounds (controls.removeFromLeft (buttonWidth).reduced (2, 0));
    const int sliderWidth = controls.getWidth() / 4;
    volume.setBounds (controls.removeFromLeft (sliderWidth));
    tune.setBounds (controls.removeFromLeft (sliderWidth));
    detune.setBounds (controls.removeFromLeft (sliderWidth));
    send.setBounds (controls);
}

MainComponent::MainComponent()
    : presetStore (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("DrumBox")
                       .getChildFile ("presets")
                       .getFullPathName()
                       .toStdString())
{
    juce::String error = deviceManager.initialiseWithDefaultDevices (0, 2);
    juce::ignoreUnused (error);
    deviceManager.addAudioCallback (this);

    auto& song = engine.getSong();

    titleLabel.setText ("DRUM BOX", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, palette::mustard);
    addAndMakeVisible (titleLabel);

    legendLabel.setColour (juce::Label::textColourId, palette::cream);
    legendLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (legendLabel);

    const char* const viewNames[] = { "DRUMS", "BASS", "PAD", "PLUCK", "FX", "CHAIN", "TRACKS", "EFFECTS" };
    for (int v = 0; v < numViews; ++v)
    {
        auto& b = viewButtons[(size_t) v];
        b.setButtonText (viewNames[v]);
        b.setTitle (juce::String ("Show ") + viewNames[v]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.onClick = [this, v] { showView ((View) v); };
        addAndMakeVisible (b);
    }

    patternLabel.setText ("PATTERN", juce::dontSendNotification);
    patternLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    patternLabel.setColour (juce::Label::textColourId, palette::cream);
    addAndMakeVisible (patternLabel);
    for (int p = 0; p < numPatterns; ++p)
    {
        auto& b = patternButtons[(size_t) p];
        b.setButtonText (juce::String (p + 1));
        b.setTitle ("Edit pattern " + juce::String (p + 1));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (2);
        b.onClick = [this, p] { selectPattern (p); };
        addAndMakeVisible (b);
    }
    patternButtons[0].setToggleState (true, juce::dontSendNotification);

    for (int t = 0; t < numTracks; ++t)
    {
        auto& label = trackLabels[(size_t) t];
        label.setText (trackNames[(size_t) t], juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, palette::cream);
        addChildComponent (label);

        for (int s = 0; s < numSteps; ++s)
        {
            auto button = std::make_unique<StepButton> (song, editing, t, s);
            addChildComponent (*button);
            steps[(size_t) (t * numSteps + s)] = std::move (button);
        }
    }

    for (int r = 0; r < numFxRows; ++r)
    {
        auto& label = fxLabels[(size_t) r];
        label.setText (fxNames[(size_t) r], juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, palette::cream);
        addChildComponent (label);

        for (int s = 0; s < numSteps; ++s)
        {
            auto button = std::make_unique<StepButton> (song, editing, r, s, true);
            addChildComponent (*button);
            fxSteps[(size_t) (r * numSteps + s)] = std::move (button);
        }
    }

    bassLabel.setText ("BASS", juce::dontSendNotification);
    bassLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    bassLabel.setColour (juce::Label::textColourId, palette::cream);
    addChildComponent (bassLabel);
    for (int s = 0; s < numSteps; ++s)
    {
        bassSteps[(size_t) s] = std::make_unique<NoteButton> (song, editing, NoteButton::Kind::Bass, 0, s);
        addChildComponent (*bassSteps[(size_t) s]);
        for (int v = 0; v < synthVoices; ++v)
        {
            auto button = std::make_unique<NoteButton> (song, editing, NoteButton::Kind::Pad, v, s);
            addChildComponent (*button);
            synthSteps[(size_t) (v * numSteps + s)] = std::move (button);
        }
        for (int v = 0; v < pluckVoices; ++v)
        {
            auto button = std::make_unique<NoteButton> (song, editing, NoteButton::Kind::Pluck, v, s);
            addChildComponent (*button);
            pluckSteps[(size_t) (v * numSteps + s)] = std::move (button);
        }
    }
    for (int v = 0; v < synthVoices; ++v)
    {
        auto& label = synthSlotLabels[(size_t) v];
        label.setText ("NOTE " + juce::String (v + 1), juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, palette::cream);
        addChildComponent (label);
    }
    for (int v = 0; v < pluckVoices; ++v)
    {
        auto& label = pluckSlotLabels[(size_t) v];
        label.setText ("NOTE " + juce::String (v + 1), juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, palette::cream);
        addChildComponent (label);
    }

    for (int c = 0; c < maxChainLength; ++c)
    {
        chainCells[(size_t) c] = std::make_unique<ChainCell> (song, editing, c);
        addChildComponent (*chainCells[(size_t) c]);
    }
    for (int t = 0; t < numMixerTracks; ++t)
    {
        auto strip = std::make_unique<TrackStrip> (song, engine.getMixer(), t, [this] (int mixerTrack) { openTrackEditor (mixerTrack); });
        strip->onAudibilityChange = [this]
        {
            for (auto& s : trackStrips)
                s->repaint();
        };
        addChildComponent (*strip);
        trackStrips[(size_t) t] = std::move (strip);
    }
    chainLengthLabel.setText ("LENGTH", juce::dontSendNotification);
    chainLengthLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    chainLengthLabel.setColour (juce::Label::textColourId, palette::cream);
    addChildComponent (chainLengthLabel);
    chainLengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    chainLengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 26);
    chainLengthSlider.setRange (1.0, maxChainLength, 1.0);
    chainLengthSlider.setTextValueSuffix (" pos.");
    chainLengthSlider.setTitle ("Chain length");
    chainLengthSlider.onValueChange = [this]
    {
        engine.getSong().setChainLength ((int) chainLengthSlider.getValue());
        repaint();
    };
    addChildComponent (chainLengthSlider);

    playButton.setClickingTogglesState (true);
    playButton.setTitle ("Play / stop");
    playButton.onClick = [this]
    {
        engine.setPlaying (playButton.getToggleState());
        updateTransportButton();
    };
    addAndMakeVisible (playButton);

    addKnob (tempo, "TEMPO", Sequencer::minTempo, Sequencer::maxTempo, 1.0, 110.0, " bpm");
    addKnob (swing, "SWING", 0.0, Sequencer::maxSwingPercent, 1.0, 0.0, " %");
    for (auto* k : { &tempo, &swing })
    {
        k->slider.setVisible (true);
        k->label.setVisible (true);
    }
    tempo.slider.onValueChange = [this] { engine.setTempo (tempo.slider.getValue()); };
    swing.slider.onValueChange = [this] { engine.setSwing (swing.slider.getValue()); };
    engine.setTempo (tempo.slider.getValue());
    engine.setSwing (swing.slider.getValue());

    addKnob (bassCutoff, "CUTOFF", 100.0, 3000.0, 10.0, 700.0, " Hz", 600.0);
    addKnob (bassDecay, "DECAY", 0.05, 1.0, 0.01, 0.25, " s");
    addKnob (osc2Semitones, "OSC2 PITCH", -12.0, 12.0, 1.0, 0.0, " st");
    addKnob (osc2Detune, "DETUNE", 0.0, 50.0, 1.0, 9.0, " ct");
    addKnob (osc2Mix, "OSC MIX", 0.0, 100.0, 1.0, 60.0, " %");
    addKnob (synthCutoff, "CUTOFF", 100.0, 8000.0, 10.0, 2200.0, " Hz", 1500.0);
    addKnob (synthResonance, "RESO", 0.0, 100.0, 1.0, 20.0, " %");
    addKnob (synthAttack, "ATTACK", 0.001, 2.0, 0.001, 0.015, " s", 0.2);
    addKnob (synthDecay, "DECAY", 0.01, 2.0, 0.01, 0.3, " s", 0.3);
    addKnob (synthSustain, "SUSTAIN", 0.0, 100.0, 1.0, 65.0, " %");
    addKnob (synthRelease, "RELEASE", 0.01, 3.0, 0.01, 0.25, " s", 0.4);
    addKnob (lfoRate, "LFO RATE", 0.1, 20.0, 0.1, 5.0, " Hz", 3.0);
    addKnob (lfoDepth, "LFO DEPTH", 0.0, 100.0, 1.0, 0.0, " %");
    addKnob (pluckCutoff, "CUTOFF", 100.0, 8000.0, 10.0, 1400.0, " Hz", 1200.0);
    addKnob (pluckDecay, "DECAY", 0.03, 1.0, 0.01, 0.18, " s", 0.2);
    addKnob (pluckResonance, "RESO", 0.0, 100.0, 1.0, 30.0, " %");
    addKnob (pluckMix, "SAW / SQUARE", 0.0, 100.0, 1.0, 40.0, " %");

    auto applyInstrumentParams = [this]
    {
        auto& b = engine.getBassParams();
        b.cutoff = (float) bassCutoff.slider.getValue();
        b.decay = (float) bassDecay.slider.getValue();
        auto& s = engine.getSynthParams();
        s.osc2Semitones = (float) osc2Semitones.slider.getValue();
        s.osc2Detune = (float) osc2Detune.slider.getValue();
        s.osc2Mix = (float) osc2Mix.slider.getValue() / 100.0f;
        s.cutoff = (float) synthCutoff.slider.getValue();
        s.resonance = (float) synthResonance.slider.getValue() / 100.0f;
        s.attack = (float) synthAttack.slider.getValue();
        s.decay = (float) synthDecay.slider.getValue();
        s.sustain = (float) synthSustain.slider.getValue() / 100.0f;
        s.release = (float) synthRelease.slider.getValue();
        s.lfoRate = (float) lfoRate.slider.getValue();
        s.lfoDepth = (float) lfoDepth.slider.getValue() / 100.0f;
        auto& p = engine.getPluckParams();
        p.cutoff = (float) pluckCutoff.slider.getValue();
        p.decay = (float) pluckDecay.slider.getValue();
        p.resonance = (float) pluckResonance.slider.getValue() / 100.0f;
        p.osc2Mix = (float) pluckMix.slider.getValue() / 100.0f;
    };
    for (auto* k : { &bassCutoff, &bassDecay, &osc2Semitones, &osc2Detune, &osc2Mix, &synthCutoff, &synthResonance,
                     &synthAttack, &synthDecay, &synthSustain, &synthRelease, &lfoRate, &lfoDepth, &pluckCutoff, &pluckDecay,
                     &pluckResonance, &pluckMix })
        k->slider.onValueChange = applyInstrumentParams;
    applyInstrumentParams();

    // EFFECTS view
    {
        static const char* const knobNames[numEffects][3] = { { "SIZE", "DAMP", "MIX" }, { "TIME", "FEEDBACK", "MIX" },
                                                              { "DRIVE", "TONE", "MIX" }, { "RATE", "DEPTH", "MIX" },
                                                              { "THRESHOLD", "RATIO", "MAKEUP" } };
        static const char* const knobSuffix[numEffects][3] = { { " %", " %", " %" }, { " ms", " %", " %" },
                                                               { " %", " %", " %" }, { " Hz", " %", " %" },
                                                               { " dB", " : 1", " dB" } };
        for (int e = 0; e < numEffects; ++e)
        {
            auto& button = effectButtons[(size_t) e];
            button.setButtonText (EffectsRack::effectName (e));
            button.setTitle (juce::String (EffectsRack::effectName (e)) + " on / off");
            button.setClickingTogglesState (true);
            button.setColour (juce::TextButton::buttonOnColourId, palette::orange);
            auto settings = engine.getEffects().settings (e);
            button.setToggleState (settings[0].value->load() >= 0.5f, juce::dontSendNotification);
            button.onClick = [this, e, &button]
            {
                engine.getEffects().settings (e)[0].value->store (button.getToggleState() ? 1.0f : 0.0f);
            };
            addChildComponent (button);

            for (int k = 0; k < 3; ++k)
            {
                const auto& setting = settings[(size_t) k + 1];
                const float scale = setting.max <= 1.0f ? 100.0f : 1.0f;
                effectScale[(size_t) e][(size_t) k] = scale;
                auto& knob = effectKnobs[(size_t) e][(size_t) k];
                const double step = scale == 100.0f ? 1.0 : (setting.max - setting.min > 100.0f ? 1.0 : 0.1);
                const double skew = std::string (setting.name) == "time" ? 300.0 : 0.0;
                addKnob (knob, knobNames[e][k], setting.min * scale, setting.max * scale, step, setting.value->load() * scale,
                         knobSuffix[e][k], skew);
                knob.slider.setTitle (juce::String (EffectsRack::effectName (e)) + " " + knobNames[e][k]);
                knob.slider.onValueChange = [this, e, k]
                {
                    auto all = engine.getEffects().settings (e);
                    all[(size_t) k + 1].value->store ((float) effectKnobs[(size_t) e][(size_t) k].slider.getValue()
                                                      / effectScale[(size_t) e][(size_t) k]);
                };
            }
        }
    }

    // preset bar
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    presetLabel.setColour (juce::Label::textColourId, palette::cream);
    addChildComponent (presetLabel);
    presetBox.setTitle ("Preset");
    presetBox.setTextWhenNothingSelected ("choose a preset");
    presetBox.onChange = [this] { applySelectedPreset(); };
    addChildComponent (presetBox);
    presetSave.setTitle ("Save the sound as a user preset");
    presetSave.onClick = [this] { askPresetName(); };
    addChildComponent (presetSave);

    songLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    songLabel.setColour (juce::Label::textColourId, palette::cream);
    songLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (songLabel);
    const struct { juce::TextButton* button; const char* title; juce::KeyPress shortcut; void (MainComponent::*action)(); } fileButtons[] = {
        { &openButton, "Open a song file", juce::KeyPress ('o', juce::ModifierKeys::commandModifier, 0), &MainComponent::openSong },
        { &saveButton, "Save the song to a file", juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0), &MainComponent::saveSong },
        { &exportButton, "Export the song as a stereo WAV file", juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0), &MainComponent::exportMix },
        { &stemsButton, "Export the mix and one WAV file per track", juce::KeyPress ('e', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0), &MainComponent::exportStems }
    };
    for (const auto& f : fileButtons)
    {
        f.button->setTitle (f.title);
        f.button->setTooltip (f.title);
        f.button->addShortcut (f.shortcut);
        f.button->onClick = [this, action = f.action] { (this->*action)(); };
        addAndMakeVisible (*f.button);
    }
    showSongName();

    loadStarterSong();
    chainLengthSlider.setValue (song.getChainLength(), juce::dontSendNotification);

    viewButtons[Drums].setToggleState (true, juce::dontSendNotification);
    updateTransportButton();
    setSize (1180, 720);
    showView (Drums);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (exportThread != nullptr)
        exportThread->waitForThreadToExit (-1);
    deviceManager.removeAudioCallback (this);
}

void MainComponent::addKnob (Knob& knob, const juce::String& text, double min, double max, double step, double initial,
                             const juce::String& suffix, double skewMidPoint)
{
    auto& slider = knob.slider;
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 24);
    slider.setRange (min, max, step);
    if (skewMidPoint > 0.0)
        slider.setSkewFactorFromMidPoint (skewMidPoint);
    slider.setValue (initial, juce::dontSendNotification);
    slider.setTextValueSuffix (suffix);
    slider.setTitle (text);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addChildComponent (slider);
    knob.label.setText (text, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    addChildComponent (knob.label);
}

void MainComponent::loadStarterSong()
{
    auto& song = engine.getSong();

    // a starter groove so the first press of PLAY is not silence
    auto& pattern = song.pattern (0);
    for (int s : { 0, 4, 8, 12 })
        pattern.set (Kick, s, StepState::Normal);
    pattern.set (Kick, 0, StepState::Accent);
    for (int s : { 4, 12 })
        pattern.set (Snare, s, StepState::Accent);
    for (int s = 0; s < numSteps; s += 2)
        pattern.set (ClosedHat, s, StepState::Normal);
    pattern.set (OpenHat, 14, StepState::Normal);

    auto& second = song.pattern (1);
    for (int t = 0; t < numTracks; ++t)
        for (int s = 0; s < numSteps; ++s)
            second.set (t, s, pattern.get (t, s));

    const auto addLine = [] (Pattern& p, int root)
    {
        p.setBass (0, root, true);
        p.setBass (3, root);
        p.setBass (6, root + 3);
        p.setBass (8, root, true);
        p.setBass (11, root + 7);
        p.setBass (14, root + 5);
        const int chord[] = { 57, 60, 64, 67 };
        for (int s : { 2, 10 })
            for (int v = 0; v < synthVoices; ++v)
                p.setSynthNote (s, v, chord[v] + (root - 33));
        p.setSynthAccent (10, true);
        const int stab[] = { 69, 72, 76 };
        for (int s : { 4, 7, 12 })
            for (int v = 0; v < 3; ++v)
                p.setPluckNote (s, v, stab[v] + (root - 33));
        p.setPluckAccent (12, true);
    };
    addLine (pattern, 33);
    addLine (second, 38);

    song.setChainLength (4);
    for (int pos = 0; pos < 4; ++pos)
        song.setChainEntry (pos, pos == 2 ? 1 : 0);
}

void MainComponent::showView (View newView)
{
    view = newView;

    const auto show = [] (auto& components, bool visible)
    {
        for (auto& c : components)
            c->setVisible (visible);
    };
    show (steps, view == Drums);
    for (auto& l : trackLabels)
        l.setVisible (view == Drums);
    show (fxSteps, view == Fx);
    for (auto& l : fxLabels)
        l.setVisible (view == Fx);
    show (bassSteps, view == Bass);
    bassLabel.setVisible (view == Bass);
    show (synthSteps, view == Pad);
    for (auto& l : synthSlotLabels)
        l.setVisible (view == Pad);
    show (pluckSteps, view == Pluck);
    for (auto& l : pluckSlotLabels)
        l.setVisible (view == Pluck);
    show (chainCells, view == Chain);
    show (trackStrips, view == Tracks);
    for (int e = 0; e < numEffects; ++e)
    {
        effectButtons[(size_t) e].setVisible (view == Effects);
        for (auto& k : effectKnobs[(size_t) e])
        {
            k.slider.setVisible (view == Effects);
            k.label.setVisible (view == Effects);
        }
    }
    presetLabel.setVisible (hasPresets());
    presetBox.setVisible (hasPresets());
    presetSave.setVisible (hasPresets());
    if (hasPresets())
        fillPresetList();
    chainLengthLabel.setVisible (view == Chain);
    chainLengthSlider.setVisible (view == Chain);
    for (auto* k : { &bassCutoff, &bassDecay })
    {
        k->slider.setVisible (view == Bass);
        k->label.setVisible (view == Bass);
    }
    for (auto* k : { &osc2Semitones, &osc2Detune, &osc2Mix, &synthCutoff, &synthResonance, &synthAttack, &synthDecay,
                     &synthSustain, &synthRelease, &lfoRate, &lfoDepth })
    {
        k->slider.setVisible (view == Pad);
        k->label.setVisible (view == Pad);
    }
    for (auto* k : { &pluckCutoff, &pluckDecay, &pluckResonance, &pluckMix })
    {
        k->slider.setVisible (view == Pluck);
        k->label.setVisible (view == Pluck);
    }

    static const char* const legends[] = {
        "click a step: off / normal / ACCENT",
        "click: note on / off   drag up or down, or wheel: pitch   right-click: accent",
        "click: note on / off   drag up or down, or wheel: pitch   right-click: accent",
        "click: note on / off   drag up or down, or wheel: pitch   right-click: accent",
        "click a step: off / normal / ACCENT",
        "click a position to give it the selected pattern   LENGTH: positions before the loop",
        "M: mute   S: solo   click a track to edit it   SEND: share sent to the effects   double-click a slider to reset it",
        "switch an effect on, then turn its knobs   the sends of the tracks (TRACKS view) feed the chain"
    };
    legendLabel.setText (legends[view], juce::dontSendNotification);
    resized();
    repaint();
}

void MainComponent::syncControlsFromEngine()
{
    const auto set = [] (Knob& k, double value) { k.slider.setValue (value, juce::dontSendNotification); };
    auto& b = engine.getBassParams();
    set (bassCutoff, b.cutoff.load());
    set (bassDecay, b.decay.load());
    auto& sy = engine.getSynthParams();
    set (osc2Semitones, sy.osc2Semitones.load());
    set (osc2Detune, sy.osc2Detune.load());
    set (osc2Mix, sy.osc2Mix.load() * 100.0);
    set (synthCutoff, sy.cutoff.load());
    set (synthResonance, sy.resonance.load() * 100.0);
    set (synthAttack, sy.attack.load());
    set (synthDecay, sy.decay.load());
    set (synthSustain, sy.sustain.load() * 100.0);
    set (synthRelease, sy.release.load());
    set (lfoRate, sy.lfoRate.load());
    set (lfoDepth, sy.lfoDepth.load() * 100.0);
    auto& p = engine.getPluckParams();
    set (pluckCutoff, p.cutoff.load());
    set (pluckDecay, p.decay.load());
    set (pluckResonance, p.resonance.load() * 100.0);
    set (pluckMix, p.osc2Mix.load() * 100.0);
    for (auto& strip : trackStrips)
        strip->refreshFromMixer();

    // what a song file also holds: tempo, swing, chain length and the effects
    set (tempo, engine.getTempo());
    set (swing, engine.getSwing());
    chainLengthSlider.setValue (engine.getSong().getChainLength(), juce::dontSendNotification);
    for (int e = 0; e < numEffects; ++e)
    {
        const auto settings = engine.getEffects().settings (e);
        effectButtons[(size_t) e].setToggleState (settings[0].value->load() >= 0.5f, juce::dontSendNotification);
        for (size_t k = 0; k < 3; ++k)
            set (effectKnobs[(size_t) e][k], settings[k + 1].value->load() * effectScale[(size_t) e][k]);
    }
    repaint();
}

juce::File MainComponent::songsFolder() const
{
    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DrumBox").getChildFile ("songs");
    folder.createDirectory();
    return folder;
}

juce::String MainComponent::songBaseName() const
{
    const auto name = songFile.getFileNameWithoutExtension();
    return isValidPresetName (name.toStdString()) ? name : juce::String ("song");
}

void MainComponent::showSongName()
{
    songLabel.setText (songFile == juce::File() ? juce::String ("untitled song") : songFile.getFileName(), juce::dontSendNotification);
}

void MainComponent::openSong()
{
    const auto start = songFile.existsAsFile() ? songFile : songsFolder();
    chooser = std::make_unique<juce::FileChooser> ("Open a song", start, "*.dbox");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() != juce::File())
                                  loadSongFrom (fc.getResult());
                          });
}

void MainComponent::loadSongFrom (const juce::File& file)
{
    if (! loadSongFile (engine, file.getFullPathName().toStdString()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open song",
                                                file.getFileName() + " is not a Drum Box song, or it is damaged.\nThe song in the window is unchanged.");
        return;
    }
    songFile = file;
    showSongName();
    selectPattern (0);
    for (int p = 0; p < numPatterns; ++p)
        patternButtons[(size_t) p].setToggleState (p == 0, juce::dontSendNotification);
    syncControlsFromEngine();
}

void MainComponent::saveSong()
{
    const auto start = songFile != juce::File() ? songFile : songsFolder().getChildFile ("my song.dbox");
    chooser = std::make_unique<juce::FileChooser> ("Save the song", start, "*.dbox");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (! file.hasFileExtension ("dbox"))
                                  file = file.getSiblingFile (file.getFileName() + ".dbox");
                              saveSongTo (file);
                          });
}

void MainComponent::saveSongTo (const juce::File& file)
{
    if (! saveSongFile (engine, file.getFullPathName().toStdString()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save song",
                                                "The song could not be saved to " + file.getFullPathName() + ".\nCheck that the folder exists and can be written to.");
        return;
    }
    songFile = file;
    showSongName();
}

// The offline render reads the song without touching the playing engine, so it runs on its own thread
// beside the music while the file buttons wait.
void MainComponent::runExport (const juce::String& title, const std::function<bool()>& work, const juce::String& done)
{
    if (exportThread != nullptr)
        return;
    struct Job : juce::Thread
    {
        Job (std::function<bool()> w, std::function<void (bool)> f) : Thread ("Export"), job (std::move (w)), finished (std::move (f)) {}
        void run() override
        {
            const bool ok = job();
            juce::MessageManager::callAsync ([f = finished, ok] { f (ok); });
        }
        std::function<bool()> job;
        std::function<void (bool)> finished;
    };
    for (auto* b : { &openButton, &saveButton, &exportButton, &stemsButton })
        b->setEnabled (false);
    songLabel.setText ("exporting...", juce::dontSendNotification);
    juce::Component::SafePointer<MainComponent> self (this);
    exportThread = std::make_unique<Job> (work, [self, title, done] (bool ok)
                                          {
                                              if (self == nullptr)
                                                  return;
                                              for (auto* b : { &self->openButton, &self->saveButton, &self->exportButton, &self->stemsButton })
                                                  b->setEnabled (true);
                                              self->showSongName();
                                              self->exportThread.reset();
                                              juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                                                                      title,
                                                                                      ok ? done : juce::String ("The export failed. Check that the folder can be written to."));
                                          });
    exportThread->startThread();
}

void MainComponent::exportMix()
{
    const auto folder = songFile.existsAsFile() ? songFile.getParentDirectory() : songsFolder();
    chooser = std::make_unique<juce::FileChooser> ("Export the song as WAV", folder.getChildFile (songBaseName() + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              if (! file.hasFileExtension ("wav"))
                                  file = file.getSiblingFile (file.getFileName() + ".wav");
                              runExport ("Export WAV", [this, file] { return exportMixWav (engine, file.getFullPathName().toStdString()); },
                                         "The song was exported to " + file.getFullPathName() + ".");
                          });
}

void MainComponent::exportStems()
{
    const auto folder = songFile.existsAsFile() ? songFile.getParentDirectory() : songsFolder();
    chooser = std::make_unique<juce::FileChooser> ("Choose where to export the stems", folder);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() == juce::File())
                                  return;
                              const auto base = songBaseName();
                              const auto target = fc.getResult().getChildFile (base + " stems");
                              runExport ("Export stems",
                                         [this, target, base] { return exportStemsWav (engine, target.getFullPathName().toStdString(), base.toStdString()); },
                                         "The mix and the twelve tracks were exported to " + target.getFullPathName() + ".");
                          });
}

void MainComponent::fillPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1;
    for (const auto& preset : PresetBank::factory (presetKind()))
        presetBox.addItem (preset.name, id++);
    const auto users = presetStore.list (presetKind());
    if (! users.empty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (const auto& name : users)
            presetBox.addItem (name, userPresetFirstId + (int) (&name - users.data()));
    }
    presetBox.setSelectedId (0, juce::dontSendNotification);
}

void MainComponent::applySelectedPreset()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0)
        return;
    PresetBank bank (engine);
    if (id < userPresetFirstId)
        bank.apply (PresetBank::factory (presetKind())[(size_t) id - 1]);
    else if (! bank.loadUser (presetStore, presetKind(), presetBox.getText().toStdString()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset",
                                                "This preset could not be read.");
        return;
    }
    syncControlsFromEngine();
}

void MainComponent::askPresetName()
{
    auto* window = new juce::AlertWindow ("Save preset", "Name of the new " + juce::String (presetKindName (presetKind())) + " preset",
                                          juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", "", "Name");
    window->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([this, window] (int result)
                                                                  {
                                                                      if (result == 1)
                                                                          saveUserPreset (window->getTextEditorContents ("name").trim());
                                                                  }),
                             true);
}

void MainComponent::saveUserPreset (const juce::String& name)
{
    if (! PresetBank (engine).saveUser (presetStore, presetKind(), name.toStdString()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset",
                                                "The name can hold letters, digits, spaces, - _ . (40 at most), and the preset folder must be writable.");
        return;
    }
    fillPresetList();
    presetBox.setText (name, juce::dontSendNotification);
}

void MainComponent::openTrackEditor (int mixerTrack)
{
    View target = Drums;
    if (mixerTrack == drumbox::mixerBass)
        target = Bass;
    else if (mixerTrack == drumbox::mixerPad)
        target = Pad;
    else if (mixerTrack == drumbox::mixerPluck)
        target = Pluck;
    else if (mixerTrack == drumbox::mixerFx)
        target = Fx;
    viewButtons[(size_t) target].setToggleState (true, juce::dontSendNotification);
    showView (target);
}

void MainComponent::selectPattern (int index)
{
    editing = index;
    repaint();
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
    if (current != shownStep)
    {
        const auto mark = [this, current] (int step, bool on)
        {
            if (step < 0)
                return;
            for (int t = 0; t < numTracks; ++t)
                steps[(size_t) (t * numSteps + step)]->setCurrent (on && current >= 0);
            for (int r = 0; r < numFxRows; ++r)
                fxSteps[(size_t) (r * numSteps + step)]->setCurrent (on && current >= 0);
            bassSteps[(size_t) step]->setCurrent (on && current >= 0);
            for (int v = 0; v < synthVoices; ++v)
                synthSteps[(size_t) (v * numSteps + step)]->setCurrent (on && current >= 0);
            for (int v = 0; v < pluckVoices; ++v)
                pluckSteps[(size_t) (v * numSteps + step)]->setCurrent (on && current >= 0);
        };
        mark (shownStep, false);
        mark (current, true);
        shownStep = current;
    }

    const int position = engine.getCurrentChainPosition();
    if (position != shownChainPosition)
    {
        if (shownChainPosition >= 0)
            chainCells[(size_t) shownChainPosition]->setCurrent (false);
        if (position >= 0)
            chainCells[(size_t) position]->setCurrent (true);
        shownChainPosition = position;
    }
    if (view == Tracks)
        for (auto& strip : trackStrips)
            strip->setCurrentPosition (position);
}

void MainComponent::paint (juce::Graphics& g)
{
    FunkLookAndFeel::paintWood (g, getLocalBounds());

    // inset dark panel behind the editor
    g.setColour (palette::brownInk.withAlpha (0.55f));
    g.fillRoundedRectangle (panel.toFloat(), 12.0f);
    g.setColour (palette::mustard);
    g.drawRoundedRectangle (panel.toFloat(), 12.0f, 3.0f);

    // racing stripe under the title, a 70's staple
    const int y = 58;
    g.setColour (palette::orange);
    g.fillRect (16, y, getWidth() - 32, 3);
    g.setColour (palette::mustard);
    g.fillRect (16, y + 5, getWidth() - 32, 3);
    g.setColour (palette::avocado);
    g.fillRect (16, y + 10, getWidth() - 32, 3);
}

void MainComponent::layoutKnobs (juce::Rectangle<int> area, const std::vector<Knob*>& knobs)
{
    const int width = area.getWidth() / (int) knobs.size();
    for (auto* k : knobs)
    {
        auto cell = area.removeFromLeft (width);
        k->label.setBounds (cell.removeFromTop (20));
        k->slider.setBounds (cell);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto header = area.removeFromTop (54);
    titleLabel.setBounds (header.removeFromLeft (230));
    auto tabs = header.removeFromRight (numViews * 82 + 12);
    for (auto& b : viewButtons)
    {
        b.setBounds (tabs.removeFromLeft (82).reduced (3, 6));
    }
    legendLabel.setBounds (header);
    area.removeFromTop (16);

    auto bank = area.removeFromTop (38);
    patternLabel.setBounds (bank.removeFromLeft (90));
    const int bankWidth = juce::jmin (bank.getWidth() - 150, 48 * numPatterns);
    for (auto& b : patternButtons)
        b.setBounds (bank.removeFromLeft (bankWidth / numPatterns).reduced (2, 2));
    area.removeFromTop (8);

    auto side = area.removeFromRight (134);
    panel = area;
    auto inner = area.reduced (10);
    if (hasPresets())
    {
        presetBar = inner.removeFromBottom (40);
        inner.removeFromBottom (6);
        presetLabel.setBounds (presetBar.removeFromLeft (90));
        presetSave.setBounds (presetBar.removeFromRight (90).reduced (2, 4));
        presetBox.setBounds (presetBar.removeFromLeft (juce::jmin (320, presetBar.getWidth())).reduced (2, 4));
    }

    side.removeFromLeft (14);
    playButton.setBounds (side.removeFromTop (64));
    side.removeFromTop (10);
    for (auto* b : { &openButton, &saveButton, &exportButton, &stemsButton })
    {
        b->setBounds (side.removeFromTop (30));
        side.removeFromTop (4);
    }
    songLabel.setBounds (side.removeFromTop (20));
    side.removeFromTop (10);
    const int knobBlock = (side.getHeight() - 14) / 2;
    auto tempoArea = side.removeFromTop (knobBlock);
    tempo.label.setBounds (tempoArea.removeFromTop (20));
    tempo.slider.setBounds (tempoArea);
    side.removeFromTop (14);
    swing.label.setBounds (side.removeFromTop (20));
    swing.slider.setBounds (side);

    const int labelWidth = 80;
    const int cellWidth = (inner.getWidth() - labelWidth) / numSteps;

    // drums: eight rows
    {
        const int rowHeight = inner.getHeight() / numTracks;
        for (int t = 0; t < numTracks; ++t)
        {
            const int y = inner.getY() + t * rowHeight;
            trackLabels[(size_t) t].setBounds (inner.getX(), y, labelWidth, rowHeight);
            for (int s = 0; s < numSteps; ++s)
                steps[(size_t) (t * numSteps + s)]->setBounds (inner.getX() + labelWidth + s * cellWidth, y, cellWidth, rowHeight);
        }
    }

    // FX: six rows
    {
        const int rowHeight = juce::jmin (inner.getHeight() / numFxRows, 90);
        for (int r = 0; r < numFxRows; ++r)
        {
            const int y = inner.getY() + r * rowHeight;
            fxLabels[(size_t) r].setBounds (inner.getX(), y, labelWidth, rowHeight);
            for (int s = 0; s < numSteps; ++s)
                fxSteps[(size_t) (r * numSteps + s)]->setBounds (inner.getX() + labelWidth + s * cellWidth, y, cellWidth, rowHeight);
        }
    }

    // bass: one tall row, its knobs below
    {
        const int rowHeight = juce::jmin (110, inner.getHeight() / 2);
        bassLabel.setBounds (inner.getX(), inner.getY(), labelWidth, rowHeight);
        for (int s = 0; s < numSteps; ++s)
            bassSteps[(size_t) s]->setBounds (inner.getX() + labelWidth + s * cellWidth, inner.getY(), cellWidth, rowHeight);
        auto knobs = inner.withTrimmedTop (rowHeight + 20).withHeight (juce::jmin (170, inner.getHeight() - rowHeight - 20));
        layoutKnobs (knobs.withWidth (juce::jmin (300, knobs.getWidth())), { &bassCutoff, &bassDecay });
    }

    // pad: four note rows, a row of knobs below
    {
        const int rowHeight = juce::jmin (56, (inner.getHeight() - 170) / synthVoices);
        for (int v = 0; v < synthVoices; ++v)
        {
            const int y = inner.getY() + v * rowHeight;
            synthSlotLabels[(size_t) v].setBounds (inner.getX(), y, labelWidth, rowHeight);
            for (int s = 0; s < numSteps; ++s)
                synthSteps[(size_t) (v * numSteps + s)]->setBounds (inner.getX() + labelWidth + s * cellWidth, y, cellWidth, rowHeight);
        }
        auto knobs = inner.withTrimmedTop (rowHeight * synthVoices + 12);
        layoutKnobs (knobs, { &osc2Semitones, &osc2Detune, &osc2Mix, &synthCutoff, &synthResonance, &synthAttack,
                              &synthDecay, &synthSustain, &synthRelease, &lfoRate, &lfoDepth });
    }

    // pluck: four note rows, a row of knobs below
    {
        const int rowHeight = juce::jmin (56, (inner.getHeight() - 170) / pluckVoices);
        for (int v = 0; v < pluckVoices; ++v)
        {
            const int y = inner.getY() + v * rowHeight;
            pluckSlotLabels[(size_t) v].setBounds (inner.getX(), y, labelWidth, rowHeight);
            for (int s = 0; s < numSteps; ++s)
                pluckSteps[(size_t) (v * numSteps + s)]->setBounds (inner.getX() + labelWidth + s * cellWidth, y, cellWidth, rowHeight);
        }
        auto knobs = inner.withTrimmedTop (rowHeight * pluckVoices + 12);
        layoutKnobs (knobs.withWidth (juce::jmin (480, knobs.getWidth())), { &pluckCutoff, &pluckDecay, &pluckResonance, &pluckMix });
    }

    // tracks: twelve strips stacked
    {
        const int stripHeight = inner.getHeight() / numMixerTracks;
        for (int t = 0; t < numMixerTracks; ++t)
            trackStrips[(size_t) t]->setBounds (inner.getX(), inner.getY() + t * stripHeight, inner.getWidth(), stripHeight);
    }

    // effects: five columns, an on button then three knobs
    {
        const int columnWidth = inner.getWidth() / numEffects;
        for (int e = 0; e < numEffects; ++e)
        {
            auto column = inner.withTrimmedLeft (e * columnWidth).withWidth (columnWidth).reduced (8, 0);
            effectButtons[(size_t) e].setBounds (column.removeFromTop (44).reduced (0, 4));
            const int knobHeight = column.getHeight() / 3;
            for (auto& k : effectKnobs[(size_t) e])
            {
                auto cell = column.removeFromTop (knobHeight);
                k.label.setBounds (cell.removeFromTop (20));
                k.slider.setBounds (cell);
            }
        }
    }

    // chain: 16 positions per row, 8 rows, then the length
    {
        auto controls = inner.withTrimmedTop (inner.getHeight() - 40);
        const int rows = maxChainLength / numSteps;
        const int rowHeight = (inner.getHeight() - 48) / rows;
        const int chainCellWidth = inner.getWidth() / numSteps;
        for (int c = 0; c < maxChainLength; ++c)
            chainCells[(size_t) c]->setBounds (inner.getX() + (c % numSteps) * chainCellWidth,
                                               inner.getY() + (c / numSteps) * rowHeight, chainCellWidth, rowHeight);
        chainLengthLabel.setBounds (controls.removeFromLeft (labelWidth));
        chainLengthSlider.setBounds (controls.removeFromLeft (juce::jmin (480, controls.getWidth())));
    }
}
