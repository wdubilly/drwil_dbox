#include "Widgets.h"

#include "FunkLookAndFeel.h"

using namespace drumbox;

juce::String noteName (int note)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}

ParamSlider::ParamSlider (juce::String settingName, int mixerTrack) : label (std::move (settingName)), track (mixerTrack)
{
    setSliderStyle (juce::Slider::LinearBar);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setSliderSnapsToMousePosition (false);
    setTitle (label);
    lookAndFeelChanged();
}

void ParamSlider::setMixerTrack (int mixerTrack)
{
    track = mixerTrack;
    lookAndFeelChanged();
}

void ParamSlider::lookAndFeelChanged()
{
    const auto& t = theme();
    setColour (juce::Slider::trackColourId, (track >= 0 ? t.trackColour (track) : t.textDim).withAlpha (0.65f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void ParamSlider::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto area = getLocalBounds().toFloat();
    g.setColour (t.cellOff);
    g.fillRoundedRectangle (area, t.cornerSize());
    Slider::paint (g);
    g.setColour (t.text);
    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (10.0f, 14.0f, area.getHeight() * 0.55f), juce::Font::bold)));
    const auto text = area.reduced (6.0f, 0.0f).toNearestInt();
    g.drawText (label, text, juce::Justification::centredLeft, true);
    g.drawText (getTextFromValue (getValue()), text, juce::Justification::centredRight, true);
}

PianoRollView::PianoRollView (Song& songToEdit, const int& editedPattern, RollTrack trackToEdit)
    : song (songToEdit), editing (editedPattern), track (trackToEdit),
      lowest (trackToEdit == RollTrack::Bass ? 36 : trackToEdit == RollTrack::Pad ? 48 : 60)
{
    setTitle (track == RollTrack::Bass ? "BASS piano roll" : track == RollTrack::Pad ? "PAD piano roll" : "PLUCK piano roll");
    lower.setTooltip ("Show one octave lower (or turn the mouse wheel)");
    higher.setTooltip ("Show one octave higher (or turn the mouse wheel)");
    lower.onClick = [this] { shiftOctave (-1); };
    higher.onClick = [this] { shiftOctave (1); };
    addAndMakeVisible (lower);
    addAndMakeVisible (higher);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click a cell: note on / off. Right-click: accent of the step.");
}

void PianoRollView::setCurrentStep (int step)
{
    if (current != step)
    {
        current = step;
        repaint();
    }
}

int PianoRollView::mixerTrack() const
{
    return track == RollTrack::Bass ? mixerBass : track == RollTrack::Pad ? mixerPad : mixerPluck;
}

juce::Rectangle<int> PianoRollView::grid() const
{
    return getLocalBounds().withTrimmedTop (28).withTrimmedLeft (keyWidth);
}

void PianoRollView::shiftOctave (int octaves)
{
    lowest = juce::jlimit (lowestNote, highestNote - (rows - 1), lowest + 12 * octaves);
    repaint();
}

bool PianoRollView::accentOf (int step) const
{
    const auto& p = song.pattern (editing);
    return track == RollTrack::Bass ? p.getBassAccent (step) : track == RollTrack::Pad ? p.getSynthAccent (step) : p.getPluckAccent (step);
}

void PianoRollView::toggleAccent (int step)
{
    auto& p = song.pattern (editing);
    if (track == RollTrack::Bass)
        p.setBass (step, p.getBassNote (step), ! p.getBassAccent (step));
    else if (track == RollTrack::Pad)
        p.setSynthAccent (step, ! p.getSynthAccent (step));
    else
        p.setPluckAccent (step, ! p.getPluckAccent (step));
}

void PianoRollView::resized()
{
    auto top = getLocalBounds().removeFromTop (26);
    higher.setBounds (top.removeFromRight (70).reduced (2, 1));
    lower.setBounds (top.removeFromRight (70).reduced (2, 1));
}

void PianoRollView::mouseDown (const juce::MouseEvent& e)
{
    const auto area = grid();
    if (! area.contains (e.getPosition()))
        return;
    const int step = (e.x - area.getX()) * numSteps / area.getWidth();
    if (e.mods.isRightButtonDown())
    {
        toggleAccent (step);
        repaint();
        return;
    }
    const int row = (e.y - area.getY()) * rows / area.getHeight();
    pianoRoll::toggleNote (song.pattern (editing), track, step, lowest + rows - 1 - row);
    repaint();
}

void PianoRollView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY != 0.0f)
        shiftOctave (wheel.deltaY > 0.0f ? 1 : -1);
}

void PianoRollView::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto area = grid();
    const float cellW = (float) area.getWidth() / numSteps;
    const float cellH = (float) area.getHeight() / rows;
    const auto& pattern = song.pattern (editing);

    g.setColour (t.textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (track == RollTrack::Bass ? "BASS NOTES" : track == RollTrack::Pad ? "PAD NOTES" : "PLUCK NOTES",
                getLocalBounds().removeFromTop (26).withTrimmedLeft (4), juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (juce::jmin (12.0f, cellH * 0.85f), juce::Font::bold)));
    for (int r = 0; r < rows; ++r)
    {
        const int note = lowest + rows - 1 - r;
        const float y = (float) area.getY() + (float) r * cellH;
        const bool black = pianoRoll::isBlackKey (note);
        g.setColour (black ? t.cellOff : t.cellOffAlt);
        g.fillRect ((float) area.getX(), y, (float) area.getWidth(), cellH);
        g.setColour (black ? t.background : t.text.withAlpha (0.9f));
        g.fillRect (0.0f, y, (float) keyWidth - 3.0f, cellH - 1.0f);
        g.setColour (black ? t.text : t.background);
        g.drawText (noteName (note), 4, (int) y, keyWidth - 10, (int) cellH, juce::Justification::centredLeft);
        g.setColour (note % 12 == 0 ? t.text.withAlpha (0.4f) : t.background.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, (float) area.getX(), (float) area.getRight());
    }
    for (int s = 0; s <= numSteps; ++s)
    {
        g.setColour (s % 4 == 0 ? t.text.withAlpha (0.4f) : t.background.withAlpha (0.5f));
        g.drawVerticalLine (area.getX() + (int) ((float) s * cellW), (float) area.getY(), (float) area.getBottom());
    }

    const auto arrow = [] (float x, float tip, bool up)
    {
        juce::Path p;
        const float base = up ? tip + 7.0f : tip - 7.0f;
        p.addTriangle (x - 5.0f, base, x + 5.0f, base, x, tip);
        return p;
    };
    for (int s = 0; s < numSteps; ++s)
    {
        const bool accent = accentOf (s);
        bool above = false, below = false;
        for (const int note : pianoRoll::notesAt (pattern, track, s))
        {
            above = above || note >= lowest + rows;
            below = below || note < lowest;
            if (note < lowest || note >= lowest + rows)
                continue;
            const auto cell = juce::Rectangle<float> ((float) area.getX() + (float) s * cellW, (float) area.getY() + (float) (lowest + rows - 1 - note) * cellH, cellW, cellH).reduced (1.5f);
            g.setColour (t.stepColour (mixerTrack(), accent ? StepState::Accent : StepState::Normal));
            g.fillRoundedRectangle (cell, t.cornerSize());
            if (! t.flat)
            {
                g.setColour (palette::brownInk);
                g.drawRoundedRectangle (cell, t.cornerSize(), 2.0f);
            }
        }
        // a note outside the shown octaves is marked at the edge so it is not lost
        g.setColour (t.text);
        const float x = (float) area.getX() + ((float) s + 0.5f) * cellW;
        if (above)
            g.fillPath (arrow (x, (float) area.getY() + 1.0f, true));
        if (below)
            g.fillPath (arrow (x, (float) area.getBottom() - 1.0f, false));
        if (accent)
            g.fillRect (x - 3.0f, (float) area.getY() - 6.0f, 6.0f, 4.0f);
    }

    if (current >= 0)
    {
        g.setColour (t.cursor.withAlpha (0.8f));
        g.drawRect (juce::Rectangle<float> ((float) area.getX() + (float) current * cellW, (float) area.getY(), cellW, (float) area.getHeight()), 2.0f);
    }
}

FxRowsView::FxRowsView (Song& songToEdit, const int& editedPattern) : song (songToEdit), editing (editedPattern)
{
    setTitle ("FX steps");
    setTooltip ("Click a step: off, normal, accent.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FxRowsView::setCurrentStep (int step)
{
    if (current != step)
    {
        current = step;
        repaint();
    }
}

juce::Rectangle<int> FxRowsView::grid() const
{
    return getLocalBounds().withTrimmedLeft (labelWidth).withTrimmedTop (26);
}

void FxRowsView::mouseDown (const juce::MouseEvent& e)
{
    const auto area = grid();
    if (! area.contains (e.getPosition()))
        return;
    const int step = (e.x - area.getX()) * numSteps / area.getWidth();
    const int row = (e.y - area.getY()) * numFxRows / area.getHeight();
    song.pattern (editing).cycleFx (row, step);
    repaint();
}

void FxRowsView::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto area = grid();
    const float cellW = (float) area.getWidth() / numSteps;
    const float cellH = (float) area.getHeight() / numFxRows;
    const auto& pattern = song.pattern (editing);

    g.setColour (t.textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("FX STEPS", getLocalBounds().removeFromTop (26).withTrimmedLeft (4), juce::Justification::centredLeft);

    for (int r = 0; r < numFxRows; ++r)
    {
        const float y = (float) area.getY() + (float) r * cellH;
        g.setColour (t.text);
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (10.0f, 14.0f, cellH * 0.5f), juce::Font::bold)));
        g.drawText (fxNames[(size_t) r], 4, (int) y, labelWidth - 6, (int) cellH, juce::Justification::centredLeft);
        for (int s = 0; s < numSteps; ++s)
        {
            const auto state = pattern.getFx (r, s);
            const auto cell = juce::Rectangle<float> ((float) area.getX() + (float) s * cellW, y, cellW, cellH).reduced (1.5f);
            g.setColour (state == StepState::Off ? ((s / 4) % 2 == 0 ? t.cellOff : t.cellOffAlt) : t.stepColour (mixerFx, state));
            g.fillRoundedRectangle (cell, t.cornerSize());
            if (state == StepState::Accent)
            {
                g.setColour (t.flat ? t.background : palette::brownInk);
                g.drawRoundedRectangle (cell.reduced (cell.getHeight() * 0.25f), 2.0f, 1.5f);
            }
        }
    }
    if (current >= 0)
    {
        g.setColour (t.cursor.withAlpha (0.8f));
        g.drawRect (juce::Rectangle<float> ((float) area.getX() + (float) current * cellW, (float) area.getY(), cellW, (float) area.getHeight()), 2.0f);
    }
}

PatternButton::PatternButton (int patternIndex) : pattern (patternIndex)
{
    setButtonText (juce::String (patternIndex + 1));
    setClickingTogglesState (true);
    setRadioGroupId (2);
    setTooltip ("Edit pattern " + juce::String (patternIndex + 1) + ". Drag it onto the chain to play it there.");
}

void PatternButton::mouseDrag (const juce::MouseEvent& e)
{
    TextButton::mouseDrag (e);
    if (dragged || e.getDistanceFromDragStart() < 8)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragged = true;
        container->startDragging ("pattern:" + juce::String (pattern), this);
    }
}

void PatternButton::mouseUp (const juce::MouseEvent& e)
{
    if (dragged)
    {
        dragged = false;
        setState (buttonNormal);
        return;
    }
    TextButton::mouseUp (e);
}

ChainCell::ChainCell (Song& songToEdit, const int& editedPattern, int positionIndex)
    : juce::Button ({}), song (songToEdit), editing (editedPattern), position (positionIndex)
{
    setTitle ("Chain position " + juce::String (positionIndex + 1));
    setTooltip ("Position " + juce::String (positionIndex + 1) + " of the chain. Click: play the edited pattern here. "
                "Drag a pattern number onto it, or drag it onto another position to swap.");
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

void ChainCell::mouseDrag (const juce::MouseEvent& e)
{
    Button::mouseDrag (e);
    if (dragged || e.getDistanceFromDragStart() < 8)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragged = true;
        container->startDragging ("chain:" + juce::String (position), this);
    }
}

void ChainCell::mouseUp (const juce::MouseEvent& e)
{
    if (dragged)
    {
        dragged = false;
        setState (buttonNormal);
        return;
    }
    Button::mouseUp (e);
}

bool ChainCell::isInterestedInDragSource (const SourceDetails& details)
{
    const auto text = details.description.toString();
    return text.startsWith ("pattern:") || (text.startsWith ("chain:") && text.fromFirstOccurrenceOf (":", false, false).getIntValue() != position);
}

void ChainCell::itemDragEnter (const SourceDetails&)
{
    dropTarget = true;
    repaint();
}

void ChainCell::itemDragExit (const SourceDetails&)
{
    dropTarget = false;
    repaint();
}

void ChainCell::itemDropped (const SourceDetails& details)
{
    dropTarget = false;
    const auto text = details.description.toString();
    const int value = text.fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (text.startsWith ("pattern:"))
        song.setChainEntry (position, value);
    else if (text.startsWith ("chain:") && value >= 0 && value < song.getChainLength())
    {
        const int other = song.getChainEntry (value);
        song.setChainEntry (value, song.getChainEntry (position));
        song.setChainEntry (position, other);
    }
    if (auto* parent = getParentComponent())
        parent->repaint();
}

void ChainCell::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& t = theme();
    const int pattern = song.getChainEntry (position);
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    if (down)
        area = area.reduced (1.5f);

    juce::Colour fill = t.flat ? t.trackColour (pattern).withMultipliedBrightness (0.8f) : palette::orange;
    if (highlighted)
        fill = fill.brighter (0.12f);
    if (! t.flat)
    {
        g.setColour (palette::brownInk.withAlpha (0.55f));
        g.fillRoundedRectangle (area.translated (0.0f, 2.0f), t.cornerSize());
    }
    g.setColour (fill);
    g.fillRoundedRectangle (area, t.cornerSize());
    if (! t.flat)
    {
        g.setColour (palette::brownInk);
        g.drawRoundedRectangle (area, t.cornerSize(), 2.0f);
    }

    g.setColour (t.flat ? t.background : palette::brownInk);
    g.setFont (juce::Font (juce::FontOptions (juce::jmin (20.0f, area.getHeight() * 0.5f), juce::Font::bold)));
    g.drawText (juce::String (pattern + 1), area.toNearestInt(), juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (juce::String (position + 1), area.reduced (4.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);

    if (current || dropTarget)
    {
        g.setColour (t.cursor);
        g.drawRoundedRectangle (area.reduced (1.0f), t.cornerSize(), dropTarget ? 3.0f : 2.0f);
    }
}

ChainStrip::ChainStrip (Song& songToEdit, const int& editedPattern) : song (songToEdit)
{
    for (int c = 0; c < maxChainLength; ++c)
    {
        cells[(size_t) c] = std::make_unique<ChainCell> (song, editedPattern, c);
        addChildComponent (*cells[(size_t) c]);
    }
}

void ChainStrip::refresh (int height)
{
    length = song.getChainLength();
    for (int c = 0; c < maxChainLength; ++c)
    {
        cells[(size_t) c]->setVisible (c < length);
        cells[(size_t) c]->setBounds (cellX (c), 0, cellWidth, height);
    }
    setSize (length * cellWidth, height);
    repaint();
}

void ChainStrip::setCurrentPosition (int position)
{
    if (shown == position)
        return;
    if (shown >= 0)
        cells[(size_t) shown]->setCurrent (false);
    if (position >= 0)
        cells[(size_t) position]->setCurrent (true);
    shown = position;
}
