#include "UiComponents.h"

namespace woody
{
LabeledKnob::LabeledKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption,
                          juce::Colour colour)
    : attachment (state, paramId, slider)
{
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setDoubleClickReturnValue (true, (double) state.getParameter (paramId)->convertFrom0to1 (state.getParameter (paramId)->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    label.setColour (juce::Label::textColourId, colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (label);
}

void LabeledKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (15));
    slider.setBounds (area);
}

//==============================================================================
MidiDragButton::MidiDragButton (const juce::String& t, bool isCompact) : text (t), compact (isCompact)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip (utf8 ("Clique e arraste até uma faixa MIDI do Ableton para criar o clipe"));
}

void MidiDragButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float radius = juce::jmin (bounds.getHeight() / 2.0f, 12.0f);

    g.setColour (colours::accent2.withAlpha (dragging ? 0.30f : 0.12f));
    g.fillRoundedRectangle (bounds, radius);

    juce::Path outline;
    outline.addRoundedRectangle (bounds, radius);
    juce::Path dashed;
    const float dashes[] = { 4.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (colours::accent2);
    g.fillPath (dashed);

    // Arrow glyph drawn as a path, so it does not depend on the font
    auto content = getLocalBounds().reduced (compact ? 6 : 12, 2).toFloat();
    const float arrowSize = compact ? 9.0f : 11.0f;
    auto arrowArea = content.removeFromLeft (arrowSize + 6.0f).withSizeKeepingCentre (arrowSize, arrowSize + 2.0f);
    juce::Path arrow;
    arrow.startNewSubPath (arrowArea.getCentreX(), arrowArea.getY());
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getBottom());
    arrow.startNewSubPath (arrowArea.getX(), arrowArea.getBottom() - arrowSize * 0.45f);
    arrow.lineTo (arrowArea.getCentreX(), arrowArea.getBottom());
    arrow.lineTo (arrowArea.getRight(), arrowArea.getBottom() - arrowSize * 0.45f);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (compact ? 11.5f : 13.0f, juce::Font::bold));
    g.drawFittedText (text, content.toNearestInt(), juce::Justification::centred, 2);
}

void MidiDragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || getSequence == nullptr)
        return;

    const auto seq = getSequence();
    if (seq.events.empty())
        return;

    dragging = true;
    repaint();

    const auto file = writeMidiFile (seq, getName != nullptr ? getName() : juce::String ("Drum Woodys"),
                                     getTiming != nullptr ? getTiming() : 0.0f);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                [safe = juce::Component::SafePointer<MidiDragButton> (this)]
                                                                {
                                                                    if (safe != nullptr)
                                                                    {
                                                                        safe->dragging = false;
                                                                        safe->repaint();
                                                                    }
                                                                });
}

void MidiDragButton::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

//==============================================================================
StepGrid::StepGrid()
{
    rows = drumRows();
}

void StepGrid::setPattern (Pattern* p, bool isEditable)
{
    pattern = p;
    editable = isEditable;
    refresh();
}

void StepGrid::setCompact (bool onlyUsedRows)
{
    compact = onlyUsedRows;
    refresh();
}

void StepGrid::refresh()
{
    rows.clear();

    for (const auto& r : drumRows())
    {
        if (compact && pattern != nullptr)
        {
            const bool used = std::any_of (pattern->hits.begin(), pattern->hits.end(), [&] (const Hit& h) { return h.note == r.note; });
            if (! used)
                continue;
        }
        rows.push_back (r);
    }

    repaint();
}

void StepGrid::setPlayStep (int step)
{
    if (step != playStep)
    {
        playStep = step;
        repaint();
    }
}

int StepGrid::getIdealHeight() const
{
    return (int) rows.size() * rowHeight + 1;
}

StepGrid::Cell StepGrid::cellAt (juce::Point<int> p) const
{
    if (pattern == nullptr || rows.empty())
        return {};

    const int row = p.y / rowHeight;
    if (row < 0 || row >= (int) rows.size())
        return {};

    if (p.x < labelWidth)
        return { -1, row };

    const float cellWidth = (float) (getWidth() - labelWidth) / (float) pattern->totalSteps();
    const int step = (int) ((float) (p.x - labelWidth) / cellWidth);
    if (step < 0 || step >= pattern->totalSteps())
        return {};

    return { step, row };
}

void StepGrid::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    if (pattern == nullptr)
        return;

    const int total = pattern->totalSteps();
    const int stepsPerBeat = juce::jmax (1, pattern->stepsPerBar / 4);
    const float cellWidth = (float) (getWidth() - labelWidth) / (float) total;

    for (int r = 0; r < (int) rows.size(); ++r)
    {
        const int y = r * rowHeight;
        const auto& row = rows[(size_t) r];
        const int slot = noteMap()[(size_t) row.note].slot;
        const auto colour = slot >= 0 ? slotColour (slot) : colours::accent;

        g.setColour (colour);
        g.fillEllipse (6.0f, (float) y + rowHeight / 2.0f - 3.0f, 6.0f, 6.0f);

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (row.name, 18, y, labelWidth - 22, rowHeight, juce::Justification::centredLeft);

        for (int s = 0; s < total; ++s)
        {
            const auto cell = juce::Rectangle<float> ((float) labelWidth + s * cellWidth, (float) y, cellWidth, (float) rowHeight).reduced (1.5f);
            const bool onBeat = (s / stepsPerBeat) % 2 == 0;

            g.setColour (onBeat ? colours::panelLight : colours::panelLight.darker (0.25f));
            g.fillRoundedRectangle (cell, 3.0f);

            if (const int velocity = pattern->velocityAt (s, row.note))
            {
                const float strength = 0.3f + 0.7f * (float) velocity / 127.0f;
                g.setColour (colour.withAlpha (strength));
                g.fillRoundedRectangle (cell, 3.0f);
            }
        }
    }

    // Bar lines
    g.setColour (colours::text.withAlpha (0.3f));
    for (int bar = 1; bar < pattern->bars; ++bar)
    {
        const float x = (float) labelWidth + (float) (bar * pattern->stepsPerBar) * cellWidth;
        g.drawLine (x, 0.0f, x, (float) getHeight(), 1.5f);
    }

    if (playStep >= 0 && playStep < total)
    {
        g.setColour (colours::text.withAlpha (0.14f));
        g.fillRect ((float) labelWidth + playStep * cellWidth, 0.0f, cellWidth, (float) getHeight());
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const auto cell = cellAt (e.getPosition());
    if (cell.row < 0)
        return;

    const auto& row = rows[(size_t) cell.row];

    if (cell.step < 0) // clicked the row name
    {
        if (onAudition != nullptr)
            onAudition (row.note, 0.8f);
        return;
    }

    if (! editable)
        return;

    if (onEditStart != nullptr)
        onEditStart();

    const int velocity = pattern->velocityAt (cell.step, row.note);

    // Right click (or Ctrl) cycles the strength of an existing hit
    if (velocity > 0 && (e.mods.isPopupMenu() || e.mods.isCtrlDown()))
    {
        const int next = velocity >= 122 ? 42 : velocity <= 42 ? 75 : velocity <= 75 ? 100 : 122;
        pattern->setHit (cell.step, row.note, next);
        lastPainted = {};
    }
    else
    {
        paintAdds = velocity == 0;
        lastPainted = {};
        applyPaint (cell);
    }

    if (paintAdds && onAudition != nullptr && pattern->velocityAt (cell.step, row.note) > 0)
        onAudition (row.note, (float) pattern->velocityAt (cell.step, row.note) / 127.0f);

    repaint();
    if (onChange != nullptr)
        onChange();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (! editable || e.mods.isPopupMenu())
        return;

    const auto cell = cellAt (e.getPosition());
    if (cell.row < 0 || cell.step < 0 || (cell.row == lastPainted.row && cell.step == lastPainted.step))
        return;

    applyPaint (cell);
    repaint();
    if (onChange != nullptr)
        onChange();
}

void StepGrid::applyPaint (Cell cell)
{
    pattern->setHit (cell.step, rows[(size_t) cell.row].note, paintAdds ? 100 : 0);
    lastPainted = cell;
}

//==============================================================================
void LevelMeter::setLevel (float linearPeak)
{
    // Instant rise, smooth fall
    const float target = juce::jlimit (0.0f, 1.2f, linearPeak);
    level = target > level ? target : level * 0.86f;

    if (target >= hold)
    {
        hold = target;
        holdTicks = 30;
    }
    else if (--holdTicks <= 0)
    {
        hold *= 0.94f;
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (bounds, 2.0f);

    // Map -48..+3 dB onto the height
    auto toY = [&] (float linear)
    {
        const float db = juce::Decibels::gainToDecibels (linear, -48.0f);
        const float proportion = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f);
        return bounds.getBottom() - proportion * bounds.getHeight();
    };

    const float top = toY (level);
    if (top < bounds.getBottom())
    {
        juce::ColourGradient gradient (colours::accent2, 0.0f, bounds.getBottom(), colours::mute, 0.0f, bounds.getY(), false);
        gradient.addColour (0.75, colours::solo);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (bounds.withTop (top), 2.0f);
    }

    if (hold > 0.001f)
    {
        g.setColour (hold >= 1.0f ? colours::mute : colours::text.withAlpha (0.7f));
        g.fillRect (bounds.getX(), toY (hold) - 1.0f, bounds.getWidth(), 2.0f);
    }
}

//==============================================================================
PillTabs::PillTabs (const juce::StringArray& n) : names (n) {}

void PillTabs::setSelected (int index, bool notify)
{
    if (index == selected || ! juce::isPositiveAndBelow (index, names.size()))
        return;

    selected = index;
    repaint();
    if (notify && onChange != nullptr)
        onChange (selected);
}

void PillTabs::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, bounds.getHeight() / 2.0f);

    const float width = bounds.getWidth() / (float) names.size();
    for (int i = 0; i < names.size(); ++i)
    {
        const auto tab = juce::Rectangle<float> (bounds.getX() + i * width, bounds.getY(), width, bounds.getHeight()).reduced (3.0f);
        if (i == selected)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (tab, tab.getHeight() / 2.0f);
        }

        g.setColour (i == selected ? colours::background : colours::textDim);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (names[i], tab, juce::Justification::centred);
    }
}

void PillTabs::mouseDown (const juce::MouseEvent& e)
{
    const int index = (int) ((float) e.x / ((float) getWidth() / (float) names.size()));
    setSelected (index);
}
} // namespace woody
