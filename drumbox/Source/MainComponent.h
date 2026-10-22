#pragma once

#include "DetailPanel.h"
#include "DrumEngine.h"
#include "EditHistory.h"
#include "FlatLookAndFeel.h"
#include "FunkLookAndFeel.h"
#include "Presets.h"
#include "SequenceView.h"
#include "Settings.h"
#include "SongExport.h"
#include "SongFile.h"
#include "Widgets.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>

// The whole window: transport bar, pattern bar, sequence, chain and the detail panel of the selected track.
class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::AudioIODeviceCallback,
                      private juce::Timer
{
public:
    // The settings are owned by the app; the skin is saved to `settingsPath` when it changes.
    MainComponent (drumbox::AppSettings& appSettings, std::string settingsPath);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void audioDeviceIOCallbackWithContext (const float* const* input, int numInputChannels, float* const* output,
                                           int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void timerCallback() override;

    void addButton (juce::TextButton& button, const juce::String& text, const juce::String& tip, std::function<void()> action);
    void togglePlay();
    void selectTrack (int mixerTrack);
    void selectPattern (int index);
    void copyPattern();
    void pastePattern();
    void clearPattern();
    void armSwap();
    void undo();
    void redo();
    void showEverythingFromEngine();
    void updateHistoryButtons();
    void loadDemo();
    void changeSkin();
    void openSong();
    void saveSong();
    void exportMix();
    void exportStems();
    void loadSongFrom (const juce::File& file);
    void saveSongTo (const juce::File& file);
    void runExport (const juce::String& title, const std::function<bool()>& work, const juce::String& done);
    void showSongName();
    juce::File songsFolder() const;
    juce::String songBaseName() const;

    drumbox::AppSettings& settings;
    std::string settingsPath;

    drumbox::DrumEngine engine;
    drumbox::EditHistory history { engine };
    juce::AudioDeviceManager deviceManager;
    drumbox::FolderPresetStore presetStore;
    FlatLookAndFeel flatLookAndFeel;
    FunkLookAndFeel funkLookAndFeel;
    juce::TooltipWindow tooltips { this, 500 };

    int editing = 0;
    int selectedTrack = 0;
    int shownStep = -2;
    int shownChainPosition = -2;
    int shownChainLength = -1;
    int ticks = 0;
    bool swapArmed = false;
    std::optional<drumbox::PatternData> clipboard;
    juce::File songFile;

    juce::TextButton playButton, undoButton, redoButton, demoButton, openButton, saveButton, exportButton, stemsButton;
    ParamSlider tempo { "TEMPO" }, swing { "SWING" };
    juce::ComboBox skinBox;
    juce::Label patternLabel, songLabel, chainLabel;
    std::array<std::unique_ptr<PatternButton>, drumbox::numPatterns> patternButtons;
    juce::TextButton copyButton, pasteButton, clearButton, swapButton;
    SequenceView sequence;
    ChainStrip chainStrip;
    juce::Viewport chainViewport;
    ParamSlider chainLength { "LENGTH" };
    DetailPanel detail;

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::Thread> exportThread;

    juce::Rectangle<int> transportBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
