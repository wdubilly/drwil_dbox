#pragma once

#include "DrumEngine.h"
#include "Presets.h"
#include "Widgets.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

// Everything about the selected track: its preset, its sound, its piano roll (or the FX steps) and the
// effects it feeds.
class DetailContent : public juce::Component
{
public:
    DetailContent (drumbox::DrumEngine& engineToEdit, const int& editedPattern, drumbox::PresetStore& store,
                   std::function<void()> onSoundChanged);

    static constexpr int minHeight = 390;

    void setTrack (int mixerTrack);
    // Shows again every value of the engine, after a preset, an undo or a song change.
    void refresh();
    void setCurrentStep (int step);
    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

private:
    enum Group { Bass = 0, Pad, Pluck, numGroups };

    struct Param
    {
        std::unique_ptr<ParamSlider> slider;
        std::function<double()> read;
    };

    Param& addParam (std::vector<Param>& list, const juce::String& name, double min, double max, double step,
                     const juce::String& suffix, double skewMidPoint, std::function<double()> read,
                     std::function<void (double)> apply, double resetValue);
    void addInstrumentParam (std::vector<Param>& list, const juce::String& name, double min, double max, double step,
                             const juce::String& suffix, double skewMidPoint, std::atomic<float>& value, float scale = 1.0f);
    drumbox::PresetKind presetKind() const;
    int groupOfTrack() const;
    std::vector<Param*> shownParams();
    void fillPresetList();
    void applySelectedPreset();
    void askPresetName();
    void saveUserPreset (const juce::String& name);

    drumbox::DrumEngine& engine;
    drumbox::PresetStore& presetStore;
    std::function<void()> soundChanged;
    int track = 0;

    juce::ComboBox presetBox;
    juce::TextButton presetSave { "SAVE" };
    int userPresetFirstId = 1000;

    std::vector<Param> mixerParams;
    std::array<std::vector<Param>, numGroups> groups;
    std::array<std::unique_ptr<PianoRollView>, 3> rolls; // bass, pad, pluck
    FxRowsView fxRows;
    juce::Label drumHint;

    std::array<juce::TextButton, drumbox::numEffects> effectButtons;
    std::array<std::array<std::unique_ptr<ParamSlider>, 3>, drumbox::numEffects> effectSliders;
    std::array<std::array<float, 3>, drumbox::numEffects> effectScale {};

    juce::Rectangle<int> soundTitle, effectsTitle;
};

// The detail panel scrolls when the window is too small to show it whole, rather than clipping it.
class DetailPanel : public juce::Component
{
public:
    DetailPanel (drumbox::DrumEngine& engineToEdit, const int& editedPattern, drumbox::PresetStore& store,
                 std::function<void()> onSoundChanged);

    void setTrack (int mixerTrack) { content.setTrack (mixerTrack); }
    void refresh() { content.refresh(); }
    void setCurrentStep (int step) { content.setCurrentStep (step); }
    void resized() override;

private:
    DetailContent content;
    juce::Viewport viewport;
};
