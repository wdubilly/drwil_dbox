#include "Theme.h"

#include "FunkLookAndFeel.h"

namespace drumbox
{
namespace
{
Theme makeDark()
{
    Theme t;
    t.flat = true;
    t.background = juce::Colour (0xff14161b);
    t.panel = juce::Colour (0xff1d2027);
    t.panelEdge = juce::Colour (0xff2f3440);
    t.text = juce::Colour (0xffe8eaf0);
    t.textDim = juce::Colour (0xff9aa1b0);
    t.cellOff = juce::Colour (0xff272b34);
    t.cellOffAlt = juce::Colour (0xff303541);
    t.cursor = juce::Colour (0xffffffff);
    t.danger = juce::Colour (0xffff5a5f);
    const juce::uint32 colours[numMixerTracks] = { 0xffff5a5f, 0xffff9f43, 0xfffeca57, 0xffc8d64a, 0xff6bd16b, 0xff2ed3a1,
                                                   0xff22c3d6, 0xff4aa3ff, 0xff6c7bff, 0xffa26bff, 0xffe062d4, 0xffff7eb6 };
    for (size_t i = 0; i < t.track.size(); ++i)
        t.track[i] = juce::Colour (colours[i]);
    return t;
}

Theme makeFunk()
{
    Theme t;
    t.flat = false;
    t.background = palette::wood;
    t.panel = palette::brownInk.withAlpha (0.55f);
    t.panelEdge = palette::mustard;
    t.text = palette::cream;
    t.textDim = palette::cream.withAlpha (0.7f);
    t.cellOff = palette::wood.darker (0.4f);
    t.cellOffAlt = palette::wood.darker (0.1f);
    t.cursor = palette::cream;
    t.danger = palette::orange;
    for (int i = 0; i < numMixerTracks; ++i)
        t.track[(size_t) i] = i == mixerPad ? palette::avocado.brighter (0.2f) : i == mixerPluck ? palette::teal : palette::orange;
    return t;
}

SkinId activeSkin = SkinId::Dark;
Theme activeTheme = makeDark();
} // namespace

juce::Colour Theme::trackColour (int mixerTrack) const
{
    return track[(size_t) juce::jlimit (0, numMixerTracks - 1, mixerTrack)];
}

juce::Colour Theme::stepColour (int mixerTrack, StepState state) const
{
    const auto base = trackColour (mixerTrack);
    if (state == StepState::Accent)
        return flat ? base.brighter (0.55f) : palette::mustard;
    return state == StepState::Normal ? (flat ? base.withMultipliedBrightness (0.8f) : base) : cellOff;
}

Theme makeTheme (SkinId skin) { return skin == SkinId::Funk ? makeFunk() : makeDark(); }

const Theme& theme() { return activeTheme; }
SkinId currentSkin() { return activeSkin; }

void setSkin (SkinId skin)
{
    activeSkin = skin;
    activeTheme = makeTheme (skin);
}

const char* skinName (SkinId skin) { return skin == SkinId::Funk ? "70's funk" : "Dark flat"; }

void paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (activeTheme.flat)
    {
        g.fillAll (activeTheme.background);
        return;
    }
    FunkLookAndFeel::paintWood (g, area);
}
} // namespace drumbox
