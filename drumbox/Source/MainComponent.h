#pragma once

#include "DrumEngine.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

class StepButton : public juce::Button
{
public:
    StepButton (drumbox::Pattern& patternToEdit, int trackIndex, int stepIndex);

    void setCurrent (bool isCurrent);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;

private:
    drumbox::Pattern& pattern;
    int track, step;
    bool current = false;
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
    void audioDeviceIOCallbackWithContext (const float* const* input, int numInputChannels, float* const* output,
                                           int numOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void timerCallback() override;

    void updateTransportButton();

    drumbox::DrumEngine engine;
    juce::AudioDeviceManager deviceManager;

    juce::TextButton playButton { "PLAY" };
    juce::Slider tempoSlider, swingSlider;
    juce::Label tempoLabel, swingLabel, titleLabel, legendLabel;
    std::array<juce::Label, drumbox::numTracks> trackLabels;
    std::array<std::unique_ptr<StepButton>, drumbox::numTracks * drumbox::numSteps> steps;
    int shownStep = -1;
};
