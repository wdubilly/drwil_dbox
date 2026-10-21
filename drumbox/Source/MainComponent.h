#pragma once

#include "DrumEngine.h"
#include "Presets.h"
#include "SongExport.h"
#include "SongFile.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

// The pattern being edited is shared by every editor: they read it at paint time.
class StepButton : public juce::Button
{
public:
    // A drum track, or a row of the FX track when isFxRow is set.
    StepButton (drumbox::Song& songToEdit, const int& editedPattern, int trackIndex, int stepIndex, bool isFxRow = false);

    void setCurrent (bool isCurrent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;

private:
    drumbox::Pattern& pattern() const { return song.pattern (editing); }

    drumbox::Song& song;
    const int& editing;
    int track, step;
    bool fx;
    bool current = false;
};

// One note of the bass (slot 0), of the pad or of the pluck (slot 0 to 3) on one step.
// Click: on / off. Drag up or down, or mouse wheel: pitch. Right click: accent of the step.
class NoteButton : public juce::Button
{
public:
    enum class Kind { Bass, Pad, Pluck };

    NoteButton (drumbox::Song& songToEdit, const int& editedPattern, Kind kindOfNote, int slotIndex, int stepIndex);

    void setCurrent (bool isCurrent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static juce::String noteName (int note);

private:
    drumbox::Pattern& pattern() const { return song.pattern (editing); }
    int getNote() const;
    void setNote (int note);
    bool getAccent() const;
    void toggleAccent();

    drumbox::Song& song;
    const int& editing;
    Kind kind;
    int slot, step;
    int lastNote;
    int dragStartNote = 0, dragStartY = 0;
    bool dragged = false;
    bool current = false;
};

// One position of the chain: shows the pattern played there, click to assign the edited pattern.
class ChainCell : public juce::Button
{
public:
    ChainCell (drumbox::Song& songToEdit, const int& editedPattern, int positionIndex);

    void setCurrent (bool isCurrent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;

private:
    drumbox::Song& song;
    const int& editing;
    int position;
    bool current = false;
};

// One line of the TRACKS view: the track's name (click to open its editor), the patterns of the
// chain where it plays, mute, solo, volume, tune and detune.
class TrackStrip : public juce::Component
{
public:
    TrackStrip (drumbox::Song& songToShow, drumbox::Mixer& mixerToEdit, int trackIndex, std::function<void (int)> onOpen);

    // Called after a mute or a solo changes, so the other strips can show what is heard.
    std::function<void()> onAudibilityChange;

    void setCurrentPosition (int position);
    // Shows again the mixer values of the track, after a preset changed them.
    void refreshFromMixer();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool playsIn (const drumbox::Pattern&) const;
    juce::Rectangle<int> mapArea() const;

    drumbox::Song& song;
    drumbox::Mixer& mixer;
    int track;
    std::function<void (int)> open;
    int currentPosition = -1;
    juce::TextButton nameButton, muteButton { "M" }, soloButton { "S" };
    juce::Slider volume, tune, detune, send;
};

class MainComponent : public juce::Component,
                      private juce::AudioIODeviceCallback,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum View { Drums = 0, Bass, Pad, Pluck, Fx, Chain, Tracks, Effects, numViews };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
    };

    void audioDeviceIOCallbackWithContext (const float* const* input, int numInputChannels, float* const* output,
                                           int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void timerCallback() override;

    void updateTransportButton();
    void showView (View view);
    void selectPattern (int index);
    void openTrackEditor (int mixerTrack);
    void addKnob (Knob& knob, const juce::String& text, double min, double max, double step, double initial,
                  const juce::String& suffix, double skewMidPoint = 0.0);
    void layoutKnobs (juce::Rectangle<int> area, const std::vector<Knob*>& knobs);
    void loadStarterSong();
    void syncControlsFromEngine();
    void fillPresetList();
    void applySelectedPreset();
    void askPresetName();
    void saveUserPreset (const juce::String& name);
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
    bool hasPresets() const { return view <= Fx; }
    drumbox::PresetKind presetKind() const { return (drumbox::PresetKind) (int) view; }

    drumbox::DrumEngine engine;
    juce::AudioDeviceManager deviceManager;
    int editing = 0;
    View view = Drums;

    juce::TextButton playButton { "PLAY" };
    std::array<juce::TextButton, numViews> viewButtons;
    std::array<juce::TextButton, drumbox::numPatterns> patternButtons;
    Knob tempo, swing;
    juce::Label titleLabel, legendLabel, patternLabel, chainLengthLabel;
    juce::Slider chainLengthSlider;

    std::array<juce::Label, drumbox::numTracks> trackLabels;
    std::array<std::unique_ptr<StepButton>, drumbox::numTracks * drumbox::numSteps> steps;
    std::array<juce::Label, drumbox::numFxRows> fxLabels;
    std::array<std::unique_ptr<StepButton>, drumbox::numFxRows * drumbox::numSteps> fxSteps;
    std::array<std::unique_ptr<NoteButton>, drumbox::numSteps> bassSteps;
    std::array<std::unique_ptr<NoteButton>, drumbox::synthVoices * drumbox::numSteps> synthSteps;
    std::array<juce::Label, drumbox::synthVoices> synthSlotLabels;
    std::array<std::unique_ptr<NoteButton>, drumbox::pluckVoices * drumbox::numSteps> pluckSteps;
    std::array<juce::Label, drumbox::pluckVoices> pluckSlotLabels;
    std::array<std::unique_ptr<ChainCell>, drumbox::maxChainLength> chainCells;
    std::array<std::unique_ptr<TrackStrip>, drumbox::numMixerTracks> trackStrips;
    juce::Label bassLabel;
    Knob bassCutoff, bassDecay;
    Knob osc2Semitones, osc2Detune, osc2Mix, synthCutoff, synthResonance, synthAttack, synthDecay, synthSustain,
         synthRelease, lfoRate, lfoDepth;
    Knob pluckCutoff, pluckDecay, pluckResonance, pluckMix;

    // EFFECTS view: per effect an on button and three knobs
    std::array<juce::TextButton, drumbox::numEffects> effectButtons;
    std::array<std::array<Knob, 3>, drumbox::numEffects> effectKnobs;
    std::array<std::array<float, 3>, drumbox::numEffects> effectScale;

    // preset bar under the editors of the tracks
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton presetSave { "SAVE" };
    drumbox::FolderPresetStore presetStore;
    int userPresetFirstId = 1000;

    // song files: open, save, export; songs live in a default folder and the user picks the file
    juce::TextButton openButton { "OPEN SONG" }, saveButton { "SAVE SONG" }, exportButton { "EXPORT WAV" },
                     stemsButton { "EXPORT STEMS" };
    juce::Label songLabel;
    juce::File songFile;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::Thread> exportThread;

    juce::Rectangle<int> panel;
    juce::Rectangle<int> presetBar;
    int shownStep = -1;
    int shownChainPosition = -1;
};
