#include "DetailPanel.h"

#include "FunkLookAndFeel.h"

using namespace drumbox;

DetailContent::DetailContent (DrumEngine& engineToEdit, const int& editedPattern, PresetStore& store,
                              std::function<void()> onSoundChanged)
    : engine (engineToEdit), presetStore (store), soundChanged (std::move (onSoundChanged)), fxRows (engineToEdit.getSong(), editedPattern)
{
    presetBox.setTextWhenNothingSelected ("pick a sound...");
    presetBox.setTooltip ("Start here: pick a ready-made sound for this track, then tune it.");
    presetBox.onChange = [this] { applySelectedPreset(); };
    addAndMakeVisible (presetBox);
    presetSave.setTooltip ("Save the current sound of this track as your own preset");
    presetSave.onClick = [this] { askPresetName(); };
    addAndMakeVisible (presetSave);

    const auto track3 = [this] { return track; };
    const auto mixerParam = [this, track3] (const juce::String& name, double min, double max, const juce::String& suffix,
                                            std::function<double (int)> read, std::function<void (int, double)> write)
    {
        addParam (mixerParams, name, min, max, 1.0, suffix, 0.0, [read, track3] { return read (track3()); },
                  [write, track3] (double v) { write (track3(), v); }, 0.0);
    };
    auto& mixer = engine.getMixer();
    mixerParam ("TUNE", -maxTuneSemitones, maxTuneSemitones, " st", [&mixer] (int t) { return (double) mixer.getTune (t); },
                [&mixer] (int t, double v) { mixer.setTune (t, (float) v); });
    mixerParam ("DETUNE", -maxDetuneCents, maxDetuneCents, " ct", [&mixer] (int t) { return (double) mixer.getDetune (t); },
                [&mixer] (int t, double v) { mixer.setDetune (t, (float) v); });
    mixerParam ("EFFECT SEND", 0.0, maxSend * 100.0, " %", [&mixer] (int t) { return (double) mixer.getSend (t) * 100.0; },
                [&mixer] (int t, double v) { mixer.setSend (t, (float) v / 100.0f); });
    mixerParams[2].slider->setTooltip ("How much of this track goes through the effects on the right");

    auto& b = engine.getBassParams();
    addInstrumentParam (groups[Bass], "CUTOFF", 100.0, 3000.0, 10.0, " Hz", 600.0, b.cutoff);
    addInstrumentParam (groups[Bass], "DECAY", 0.05, 1.0, 0.01, " s", 0.0, b.decay);
    auto& s = engine.getSynthParams();
    addInstrumentParam (groups[Pad], "OSC2 PITCH", -12.0, 12.0, 1.0, " st", 0.0, s.osc2Semitones);
    addInstrumentParam (groups[Pad], "OSC2 DETUNE", 0.0, 50.0, 1.0, " ct", 0.0, s.osc2Detune);
    addInstrumentParam (groups[Pad], "OSC MIX", 0.0, 100.0, 1.0, " %", 0.0, s.osc2Mix, 100.0f);
    addInstrumentParam (groups[Pad], "CUTOFF", 100.0, 8000.0, 10.0, " Hz", 1500.0, s.cutoff);
    addInstrumentParam (groups[Pad], "RESONANCE", 0.0, 100.0, 1.0, " %", 0.0, s.resonance, 100.0f);
    addInstrumentParam (groups[Pad], "ATTACK", 0.001, 2.0, 0.001, " s", 0.2, s.attack);
    addInstrumentParam (groups[Pad], "DECAY", 0.01, 2.0, 0.01, " s", 0.3, s.decay);
    addInstrumentParam (groups[Pad], "SUSTAIN", 0.0, 100.0, 1.0, " %", 0.0, s.sustain, 100.0f);
    addInstrumentParam (groups[Pad], "RELEASE", 0.01, 3.0, 0.01, " s", 0.4, s.release);
    addInstrumentParam (groups[Pad], "LFO RATE", 0.1, 20.0, 0.1, " Hz", 3.0, s.lfoRate);
    addInstrumentParam (groups[Pad], "LFO DEPTH", 0.0, 100.0, 1.0, " %", 0.0, s.lfoDepth, 100.0f);
    auto& p = engine.getPluckParams();
    addInstrumentParam (groups[Pluck], "CUTOFF", 100.0, 8000.0, 10.0, " Hz", 1200.0, p.cutoff);
    addInstrumentParam (groups[Pluck], "DECAY", 0.03, 1.0, 0.01, " s", 0.2, p.decay);
    addInstrumentParam (groups[Pluck], "RESONANCE", 0.0, 100.0, 1.0, " %", 0.0, p.resonance, 100.0f);
    addInstrumentParam (groups[Pluck], "SAW / SQUARE", 0.0, 100.0, 1.0, " %", 0.0, p.osc2Mix, 100.0f);

    const RollTrack rollTracks[] = { RollTrack::Bass, RollTrack::Pad, RollTrack::Pluck };
    for (size_t r = 0; r < rolls.size(); ++r)
    {
        rolls[r] = std::make_unique<PianoRollView> (engine.getSong(), editedPattern, rollTracks[r]);
        addChildComponent (*rolls[r]);
    }
    addChildComponent (fxRows);

    drumHint.setText ("Click the steps of this drum in the grid above.\nPick a sound on top, then tune it on the right.",
                      juce::dontSendNotification);
    drumHint.setJustificationType (juce::Justification::centredLeft);
    drumHint.setMinimumHorizontalScale (1.0f);
    addChildComponent (drumHint);

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
        button.setTooltip (juce::String (EffectsRack::effectName (e)) + " on / off. Every track feeds it with its EFFECT SEND.");
        button.setClickingTogglesState (true);
        button.onClick = [this, e, &button]
        {
            engine.getEffects().settings (e)[0].value->store (button.getToggleState() ? 1.0f : 0.0f);
        };
        addAndMakeVisible (button);

        auto settings = engine.getEffects().settings (e);
        for (int k = 0; k < 3; ++k)
        {
            const auto& setting = settings[(size_t) k + 1];
            const float scale = setting.max <= 1.0f ? 100.0f : 1.0f;
            effectScale[(size_t) e][(size_t) k] = scale;
            auto slider = std::make_unique<ParamSlider> (knobNames[e][k]);
            slider->setRange (setting.min * scale, setting.max * scale,
                              scale == 100.0f ? 1.0 : (setting.max - setting.min > 100.0f ? 1.0 : 0.1));
            if (std::string (setting.name) == "time")
                slider->setSkewFactorFromMidPoint (300.0);
            slider->setTextValueSuffix (knobSuffix[e][k]);
            slider->setTitle (juce::String (EffectsRack::effectName (e)) + " " + knobNames[e][k]);
            slider->setTooltip (juce::String (EffectsRack::effectName (e)) + " " + knobNames[e][k]);
            slider->onValueChange = [this, e, k]
            {
                auto all = engine.getEffects().settings (e);
                all[(size_t) k + 1].value->store ((float) effectSliders[(size_t) e][(size_t) k]->getValue()
                                                  / effectScale[(size_t) e][(size_t) k]);
            };
            addAndMakeVisible (*slider);
            effectSliders[(size_t) e][(size_t) k] = std::move (slider);
        }
    }

    refresh();
    setTrack (0);
    lookAndFeelChanged();
}

DetailContent::Param& DetailContent::addParam (std::vector<Param>& list, const juce::String& name, double min, double max,
                                               double step, const juce::String& suffix, double skewMidPoint,
                                               std::function<double()> read, std::function<void (double)> apply, double resetValue)
{
    Param param;
    param.slider = std::make_unique<ParamSlider> (name);
    param.slider->setRange (min, max, step);
    if (skewMidPoint > 0.0)
        param.slider->setSkewFactorFromMidPoint (skewMidPoint);
    param.slider->setTextValueSuffix (suffix);
    param.slider->setTooltip (name + ". Double-click to reset.");
    param.slider->setValue (read(), juce::dontSendNotification);
    param.slider->setDoubleClickReturnValue (true, resetValue != 0.0 ? resetValue : juce::jlimit (min, max, read()));
    param.slider->onValueChange = [slider = param.slider.get(), apply = std::move (apply)] { apply (slider->getValue()); };
    param.read = std::move (read);
    addChildComponent (*param.slider);
    list.push_back (std::move (param));
    return list.back();
}

void DetailContent::addInstrumentParam (std::vector<Param>& list, const juce::String& name, double min, double max,
                                        double step, const juce::String& suffix, double skewMidPoint,
                                        std::atomic<float>& value, float scale)
{
    const double initial = value.load() * scale;
    addParam (list, name, min, max, step, suffix, skewMidPoint, [&value, scale] { return (double) value.load() * scale; },
              [&value, scale] (double v) { value.store ((float) v / scale); }, initial);
}

int DetailContent::groupOfTrack() const
{
    return track == mixerBass ? Bass : track == mixerPad ? Pad : track == mixerPluck ? Pluck : -1;
}

PresetKind DetailContent::presetKind() const
{
    if (track < numTracks)
        return PresetKind::DrumKit;
    return track == mixerBass ? PresetKind::Bass : track == mixerPad ? PresetKind::Pad
         : track == mixerPluck ? PresetKind::Pluck : PresetKind::Fx;
}

std::vector<DetailContent::Param*> DetailContent::shownParams()
{
    std::vector<Param*> shown;
    for (auto& p : mixerParams)
        shown.push_back (&p);
    if (const int g = groupOfTrack(); g >= 0)
        for (auto& p : groups[(size_t) g])
            shown.push_back (&p);
    return shown;
}

void DetailContent::setTrack (int mixerTrack)
{
    track = juce::jlimit (0, numMixerTracks - 1, mixerTrack);
    const int group = groupOfTrack();
    for (size_t r = 0; r < rolls.size(); ++r)
        rolls[r]->setVisible ((int) r == group);
    fxRows.setVisible (track == mixerFx);
    drumHint.setVisible (track < numTracks);
    for (size_t g = 0; g < groups.size(); ++g)
        for (auto& p : groups[g])
            p.slider->setVisible ((int) g == group);
    for (auto& p : mixerParams)
    {
        p.slider->setVisible (true);
        p.slider->setMixerTrack (track);
        p.slider->setValue (p.read(), juce::dontSendNotification);
    }
    for (auto& g : groups)
        for (auto& p : g)
            p.slider->setMixerTrack (track);
    fillPresetList();
    resized();
    repaint();
}

void DetailContent::refresh()
{
    for (auto& p : mixerParams)
        p.slider->setValue (p.read(), juce::dontSendNotification);
    for (auto& g : groups)
        for (auto& p : g)
            p.slider->setValue (p.read(), juce::dontSendNotification);
    for (int e = 0; e < numEffects; ++e)
    {
        auto settings = engine.getEffects().settings (e);
        effectButtons[(size_t) e].setToggleState (settings[0].value->load() >= 0.5f, juce::dontSendNotification);
        for (size_t k = 0; k < 3; ++k)
            effectSliders[(size_t) e][k]->setValue (settings[k + 1].value->load() * effectScale[(size_t) e][k], juce::dontSendNotification);
    }
    repaint();
}

void DetailContent::setCurrentStep (int step)
{
    for (auto& r : rolls)
        r->setCurrentStep (step);
    fxRows.setCurrentStep (step);
}

void DetailContent::lookAndFeelChanged()
{
    const auto& t = theme();
    drumHint.setColour (juce::Label::textColourId, t.textDim);
    for (auto& b : effectButtons)
        b.setColour (juce::TextButton::buttonOnColourId, t.flat ? t.trackColour (mixerBass) : palette::orange);
}

void DetailContent::fillPresetList()
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

void DetailContent::applySelectedPreset()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0)
        return;
    PresetBank bank (engine);
    if (id < userPresetFirstId)
        bank.apply (PresetBank::factory (presetKind())[(size_t) id - 1]);
    else if (! bank.loadUser (presetStore, presetKind(), presetBox.getText().toStdString()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", "This preset could not be read.");
        return;
    }
    soundChanged();
}

void DetailContent::askPresetName()
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

void DetailContent::saveUserPreset (const juce::String& name)
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

void DetailContent::paint (juce::Graphics& g)
{
    const auto& t = theme();
    g.setColour (t.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), t.cornerSize() + 2.0f);
    g.setColour (t.panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), t.cornerSize() + 2.0f, t.flat ? 1.0f : 2.0f);

    const auto colour = t.trackColour (track);
    g.setColour (colour);
    g.fillRoundedRectangle (12.0f, 12.0f, 5.0f, 24.0f, 2.0f);
    g.setColour (t.text);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    g.drawText (mixerTrackNames[(size_t) track], 24, 8, 120, 32, juce::Justification::centredLeft);

    g.setColour (t.textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("SOUND", soundTitle, juce::Justification::centredLeft);
    g.drawText ("EFFECTS  (shared by all tracks, each track sends its share)", effectsTitle, juce::Justification::centredLeft);
}

void DetailContent::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    auto header = area.removeFromTop (34);
    header.removeFromLeft (150);
    presetBox.setBounds (header.removeFromLeft (juce::jmin (300, header.getWidth() - 80)).reduced (0, 2));
    header.removeFromLeft (6);
    presetSave.setBounds (header.removeFromLeft (70).reduced (0, 2));
    area.removeFromTop (4);

    // effects: five columns along the bottom, a toggle over three bars
    constexpr int barHeight = 22;
    auto effects = area.removeFromBottom (18 + 26 + 3 * barHeight + 6);
    effectsTitle = effects.removeFromTop (18);
    const int columnWidth = effects.getWidth() / numEffects;
    for (int e = 0; e < numEffects; ++e)
    {
        auto column = effects.removeFromLeft (columnWidth).reduced (4, 0);
        effectButtons[(size_t) e].setBounds (column.removeFromTop (26).reduced (0, 1));
        for (auto& slider : effectSliders[(size_t) e])
            slider->setBounds (column.removeFromTop (barHeight).reduced (0, 1));
    }
    area.removeFromBottom (6);

    auto left = area.removeFromLeft (area.getWidth() * 46 / 100);
    area.removeFromLeft (10);
    for (auto& r : rolls)
        r->setBounds (left);
    fxRows.setBounds (left);
    drumHint.setBounds (left);

    soundTitle = area.removeFromTop (18);
    const auto shown = shownParams();
    const int rowHeight = 24;
    const int perColumn = juce::jmax (1, area.getHeight() / rowHeight);
    const int columns = juce::jmax (1, ((int) shown.size() + perColumn - 1) / perColumn);
    const int width = area.getWidth() / columns;
    for (size_t i = 0; i < shown.size(); ++i)
    {
        const int c = (int) i / perColumn, r = (int) i % perColumn;
        shown[i]->slider->setBounds (area.getX() + c * width + 2, area.getY() + r * rowHeight + 1, width - 4, rowHeight - 2);
    }
}

DetailPanel::DetailPanel (DrumEngine& engineToEdit, const int& editedPattern, PresetStore& store, std::function<void()> onSoundChanged)
    : content (engineToEdit, editedPattern, store, std::move (onSoundChanged))
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void DetailPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    const int height = juce::jmax (DetailContent::minHeight, viewport.getMaximumVisibleHeight());
    content.setSize (viewport.getMaximumVisibleWidth(), height);
}
