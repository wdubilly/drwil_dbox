#include "MainComponent.h"

#include "DemoSong.h"

using namespace drumbox;

namespace
{
constexpr int margin = 8;
constexpr int transportHeight = 48;
constexpr int patternBarHeight = 34;
constexpr int chainBarHeight = 46;
constexpr int chainCellHeight = 30;
constexpr int gap = 6;

juce::File presetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("DrumBox").getChildFile ("presets");
}
} // namespace

MainComponent::MainComponent (AppSettings& appSettings, std::string pathOfSettings)
    : settings (appSettings),
      settingsPath (std::move (pathOfSettings)),
      presetStore (presetsFolder().getFullPathName().toStdString()),
      sequence (engine, editing, [this] (int t) { selectTrack (t); }),
      chainStrip (engine.getSong(), editing),
      detail (engine, editing, presetStore, [this] { showEverythingFromEngine(); })
{
    juce::String error = deviceManager.initialiseWithDefaultDevices (0, 2);
    juce::ignoreUnused (error);
    deviceManager.addAudioCallback (this);

    setWantsKeyboardFocus (true);

    setSkin (settings.skin);
    juce::LookAndFeel::setDefaultLookAndFeel (settings.skin == SkinId::Dark ? static_cast<juce::LookAndFeel*> (&flatLookAndFeel)
                                                                            : static_cast<juce::LookAndFeel*> (&funkLookAndFeel));

    addButton (playButton, "PLAY", "Start or stop the music (space bar)", [this] { togglePlay(); });
    playButton.setClickingTogglesState (false);
    playButton.setTitle ("Play / stop");
    addButton (undoButton, "UNDO", "Take back the last change (Ctrl+Z)", [this] { undo(); });
    addButton (redoButton, "REDO", "Bring back what you took back (Ctrl+Y)", [this] { redo(); });
    addButton (demoButton, "DEMO", "Load the demo song (you can undo it)", [this] { loadDemo(); });
    addButton (openButton, "OPEN", "Open a song file (Ctrl+O)", [this] { openSong(); });
    addButton (saveButton, "SAVE", "Save the song to a file (Ctrl+S)", [this] { saveSong(); });
    addButton (exportButton, "WAV", "Export the song as a WAV file (Ctrl+E)", [this] { exportMix(); });
    addButton (stemsButton, "STEMS", "Export the mix and every track as WAV files (Ctrl+Shift+E)", [this] { exportStems(); });
    openButton.addShortcut (juce::KeyPress ('o', juce::ModifierKeys::commandModifier, 0));
    saveButton.addShortcut (juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0));
    exportButton.addShortcut (juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0));
    stemsButton.addShortcut (juce::KeyPress ('e', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));

    tempo.setRange (Sequencer::minTempo, Sequencer::maxTempo, 1.0);
    tempo.setTextValueSuffix (" bpm");
    tempo.setTooltip ("Speed of the song. Double-click to reset.");
    tempo.setDoubleClickReturnValue (true, 110.0);
    tempo.setValue (engine.getTempo(), juce::dontSendNotification);
    tempo.onValueChange = [this] { engine.setTempo (tempo.getValue()); };
    addAndMakeVisible (tempo);
    swing.setRange (0.0, Sequencer::maxSwingPercent, 1.0);
    swing.setTextValueSuffix (" %");
    swing.setTooltip ("Delays every second step for a shuffle feel. Double-click to reset.");
    swing.setDoubleClickReturnValue (true, 0.0);
    swing.setValue (engine.getSwing(), juce::dontSendNotification);
    swing.onValueChange = [this] { engine.setSwing (swing.getValue()); };
    addAndMakeVisible (swing);

    skinBox.addItem ("DARK", 1);
    skinBox.addItem ("70'S FUNK", 2);
    skinBox.setSelectedId (settings.skin == SkinId::Dark ? 1 : 2, juce::dontSendNotification);
    skinBox.setTooltip ("Look of the window");
    skinBox.setTitle ("Skin");
    skinBox.setWantsKeyboardFocus (false);
    skinBox.onChange = [this] { changeSkin(); };
    addAndMakeVisible (skinBox);

    patternLabel.setText ("PATTERN", juce::dontSendNotification);
    addAndMakeVisible (patternLabel);
    for (int p = 0; p < numPatterns; ++p)
    {
        auto button = std::make_unique<PatternButton> (p);
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, p] { selectPattern (p); };
        addAndMakeVisible (*button);
        patternButtons[(size_t) p] = std::move (button);
    }
    patternButtons[0]->setToggleState (true, juce::dontSendNotification);
    addButton (copyButton, "COPY", "Copy the edited pattern", [this] { copyPattern(); });
    addButton (pasteButton, "PASTE", "Paste the copied pattern over the edited one", [this] { pastePattern(); });
    pasteButton.setEnabled (false);
    addButton (clearButton, "CLEAR", "Empty the edited pattern (asks first)", [this] { clearPattern(); });
    addButton (swapButton, "SWAP", "Click SWAP, then another pattern number: the two exchange their contents", [this] { armSwap(); });
    swapButton.setClickingTogglesState (true);
    songLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (songLabel);
    showSongName();

    addAndMakeVisible (sequence);

    chainLabel.setText ("CHAIN", juce::dontSendNotification);
    addAndMakeVisible (chainLabel);
    chainViewport.setViewedComponent (&chainStrip, false);
    chainViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (chainViewport);
    chainLength.setRange (1.0, maxChainLength, 1.0);
    chainLength.setTextValueSuffix (" pos.");
    chainLength.setTooltip ("How many positions of the chain play before the song loops. Click a position to give it the edited pattern, or drag a pattern number onto it.");
    chainLength.onValueChange = [this]
    {
        engine.getSong().setChainLength ((int) chainLength.getValue());
        repaint();
    };
    addAndMakeVisible (chainLength);

    addAndMakeVisible (detail);

    if (settings.firstLaunch)
    {
        loadDemoSong (engine);
        settings.firstLaunch = false;
        saveSettings (settings, settingsPath);
    }
    selectTrack (0);
    history.reset();
    showEverythingFromEngine();
    lookAndFeelChanged();
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (exportThread != nullptr)
        exportThread->waitForThreadToExit (-1);
    deviceManager.removeAudioCallback (this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void MainComponent::addButton (juce::TextButton& button, const juce::String& text, const juce::String& tip, std::function<void()> action)
{
    button.setButtonText (text);
    button.setTooltip (tip);
    button.setWantsKeyboardFocus (false);
    button.onClick = std::move (action);
    addAndMakeVisible (button);
}

void MainComponent::togglePlay()
{
    const bool play = ! engine.isPlaying();
    engine.setPlaying (play);
    playButton.setButtonText (play ? "STOP" : "PLAY");
    playButton.setToggleState (play, juce::dontSendNotification);
}

void MainComponent::selectTrack (int mixerTrack)
{
    selectedTrack = mixerTrack;
    sequence.setSelectedTrack (mixerTrack);
    detail.setTrack (mixerTrack);
}

void MainComponent::selectPattern (int index)
{
    if (swapArmed)
    {
        engine.getSong().swapPatterns (editing, index);
        swapArmed = false;
        swapButton.setToggleState (false, juce::dontSendNotification);
        patternButtons[(size_t) editing]->setToggleState (true, juce::dontSendNotification);
        repaint();
        return;
    }
    editing = index;
    repaint();
}

void MainComponent::copyPattern()
{
    clipboard = engine.getSong().copyPattern (editing);
    pasteButton.setEnabled (true);
}

void MainComponent::pastePattern()
{
    if (! clipboard)
        return;
    engine.getSong().pastePattern (editing, *clipboard);
    repaint();
}

void MainComponent::clearPattern()
{
    juce::Component::SafePointer<MainComponent> self (this);
    const int target = editing;
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Clear pattern",
                                        "Clear pattern " + juce::String (target + 1) + "? Its drums, bass, pad, pluck and FX are emptied. You can undo it.",
                                        "Clear", "Cancel", this,
                                        juce::ModalCallbackFunction::create ([self, target] (int result)
                                        {
                                            if (self != nullptr && result == 1)
                                            {
                                                self->engine.getSong().clearPattern (target);
                                                self->repaint();
                                            }
                                        }));
}

void MainComponent::armSwap()
{
    swapArmed = swapButton.getToggleState();
}

void MainComponent::undo()
{
    if (history.undo())
        showEverythingFromEngine();
}

void MainComponent::redo()
{
    if (history.redo())
        showEverythingFromEngine();
}

void MainComponent::updateHistoryButtons()
{
    undoButton.setEnabled (history.canUndo());
    redoButton.setEnabled (history.canRedo());
}

// What a song change (undo, preset, open, demo) can have moved: every control shows the engine again.
void MainComponent::showEverythingFromEngine()
{
    tempo.setValue (engine.getTempo(), juce::dontSendNotification);
    swing.setValue (engine.getSwing(), juce::dontSendNotification);
    chainLength.setValue (engine.getSong().getChainLength(), juce::dontSendNotification);
    sequence.refreshFromMixer();
    detail.refresh();
    shownChainLength = -1;
    updateHistoryButtons();
    repaint();
}

void MainComponent::loadDemo()
{
    history.commit();
    loadDemoSong (engine);
    history.commit();
    songFile = juce::File();
    showSongName();
    showEverythingFromEngine();
}

void MainComponent::changeSkin()
{
    settings.skin = skinBox.getSelectedId() == 2 ? SkinId::Funk : SkinId::Dark;
    setSkin (settings.skin);
    juce::LookAndFeel::setDefaultLookAndFeel (settings.skin == SkinId::Dark ? static_cast<juce::LookAndFeel*> (&flatLookAndFeel)
                                                                            : static_cast<juce::LookAndFeel*> (&funkLookAndFeel));
    sendLookAndFeelChange();
    saveSettings (settings, settingsPath);
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
    editing = 0;
    for (int p = 0; p < numPatterns; ++p)
        patternButtons[(size_t) p]->setToggleState (p == 0, juce::dontSendNotification);
    history.reset();
    showEverythingFromEngine();
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
    for (auto* b : { &openButton, &saveButton, &exportButton, &stemsButton, &demoButton })
        b->setEnabled (false);
    songLabel.setText ("exporting...", juce::dontSendNotification);
    juce::Component::SafePointer<MainComponent> self (this);
    exportThread = std::make_unique<Job> (work, [self, title, done] (bool ok)
                                          {
                                              if (self == nullptr)
                                                  return;
                                              for (auto* b : { &self->openButton, &self->saveButton, &self->exportButton, &self->stemsButton, &self->demoButton })
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

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlay();
        return true;
    }
    if (mods.isCommandDown() && (key.getKeyCode() == 'z' || key.getKeyCode() == 'Z'))
    {
        if (mods.isShiftDown())
            redo();
        else
            undo();
        return true;
    }
    if (mods.isCommandDown() && (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y'))
    {
        redo();
        return true;
    }
    return false;
}

void MainComponent::timerCallback()
{
    // Every change is recorded as a step once the mouse is up, so a drag is one undo and no widget needs a hook.
    if (++ticks % 3 == 0 && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        if (history.commit())
            updateHistoryButtons();

    const int current = engine.getCurrentStep();
    if (current != shownStep)
    {
        shownStep = current;
        sequence.setCurrentStep (current);
        detail.setCurrentStep (current);
    }

    if (engine.getSong().getChainLength() != shownChainLength)
    {
        shownChainLength = engine.getSong().getChainLength();
        chainStrip.refresh (chainCellHeight);
        shownChainPosition = -2;
    }
    const int position = engine.getCurrentChainPosition();
    if (position != shownChainPosition)
    {
        shownChainPosition = position;
        chainStrip.setCurrentPosition (position);
        if (position >= 0)
        {
            const int x = chainStrip.cellX (position);
            const int width = chainViewport.getMaximumVisibleWidth();
            const int left = chainViewport.getViewPositionX();
            if (x < left || x + ChainStrip::cellWidth > left + width)
                chainViewport.setViewPosition (juce::jmax (0, x - width / 2), 0);
        }
    }
    if (engine.isPlaying() != (playButton.getButtonText() == "STOP"))
        playButton.setButtonText (engine.isPlaying() ? "STOP" : "PLAY");
}

void MainComponent::lookAndFeelChanged()
{
    const auto& t = theme();
    for (auto* label : { &patternLabel, &chainLabel })
    {
        label->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        label->setColour (juce::Label::textColourId, t.textDim);
    }
    songLabel.setColour (juce::Label::textColourId, t.text);
    playButton.setColour (juce::TextButton::buttonColourId, t.flat ? t.trackColour (mixerBass) : palette::orange);
    playButton.setColour (juce::TextButton::textColourOffId, t.flat ? juce::Colours::black : palette::cream);
    repaint();
}

void MainComponent::paint (juce::Graphics& g)
{
    paintBackground (g, getLocalBounds());
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (margin);

    auto bar = area.removeFromTop (transportHeight);
    playButton.setBounds (bar.removeFromLeft (84));
    bar.removeFromLeft (gap);
    tempo.setBounds (bar.removeFromLeft (160).reduced (0, 9));
    bar.removeFromLeft (gap);
    swing.setBounds (bar.removeFromLeft (130).reduced (0, 9));
    bar.removeFromLeft (gap * 2);
    skinBox.setBounds (bar.removeFromRight (112).reduced (0, 9));
    bar.removeFromRight (gap);
    for (auto* b : { &undoButton, &redoButton })
    {
        b->setBounds (bar.removeFromLeft (54).reduced (0, 7));
        bar.removeFromLeft (3);
    }
    bar.removeFromLeft (gap);
    for (auto* b : { &demoButton, &openButton, &saveButton, &exportButton, &stemsButton })
    {
        b->setBounds (bar.removeFromLeft (b == &stemsButton ? 62 : 54).reduced (0, 7));
        bar.removeFromLeft (3);
    }
    area.removeFromTop (gap);

    auto patterns = area.removeFromTop (patternBarHeight);
    patternLabel.setBounds (patterns.removeFromLeft (70));
    for (auto& b : patternButtons)
        b->setBounds (patterns.removeFromLeft (32).reduced (1, 2));
    patterns.removeFromLeft (gap * 2);
    for (auto* b : { &copyButton, &pasteButton, &clearButton, &swapButton })
    {
        b->setBounds (patterns.removeFromLeft (56).reduced (1, 2));
    }
    songLabel.setBounds (patterns);
    area.removeFromTop (gap);

    const int remaining = area.getHeight() - chainBarHeight - 2 * gap;
    const int detailHeight = juce::jmax (200, remaining * 45 / 100);
    detail.setBounds (area.removeFromBottom (detailHeight));
    area.removeFromBottom (gap);
    auto chainBar = area.removeFromBottom (chainBarHeight);
    area.removeFromBottom (gap);
    sequence.setBounds (area);

    chainLabel.setBounds (chainBar.removeFromLeft (60).withHeight (chainCellHeight));
    chainLength.setBounds (chainBar.removeFromRight (190).withHeight (chainCellHeight).reduced (0, 3));
    chainBar.removeFromRight (gap);
    chainViewport.setBounds (chainBar);
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
