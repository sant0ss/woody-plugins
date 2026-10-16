#include "UiComponents.h"

namespace woody
{
LabeledKnob::LabeledKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption)
    : attachment (state, paramId, slider)
{
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void LabeledKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (16));
    slider.setBounds (area);
}

//==============================================================================
MidiDragButton::MidiDragButton (const juce::String& t) : text (t)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip (utf8 ("Clique e arraste até uma faixa MIDI do Ableton para criar o clipe"));
}

void MidiDragButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.5f);

    g.setColour (colours::accent.withAlpha (dragging ? 0.35f : 0.15f));
    g.fillRoundedRectangle (bounds, 6.0f);

    juce::Path outline;
    outline.addRoundedRectangle (bounds, 6.0f);
    juce::Path dashed;
    const float dashes[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (colours::accent);
    g.fillPath (dashed);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (text, getLocalBounds().reduced (6, 2), juce::Justification::centred, 2);
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

    const auto file = writeMidiFile (seq, getName != nullptr ? getName() : juce::String ("Drum Woodys"));
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

        g.setColour (r % 2 == 0 ? colours::panel : colours::panel.brighter (0.04f));
        g.fillRect (0, y, getWidth(), rowHeight);

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (row.name, 6, y, labelWidth - 10, rowHeight, juce::Justification::centredLeft);

        for (int s = 0; s < total; ++s)
        {
            const auto cell = juce::Rectangle<float> ((float) labelWidth + s * cellWidth, (float) y, cellWidth, (float) rowHeight).reduced (1.5f);
            const bool onBeat = (s / stepsPerBeat) % 2 == 0;

            g.setColour (onBeat ? colours::background.brighter (0.06f) : colours::background);
            g.fillRoundedRectangle (cell, 2.0f);

            if (const int velocity = pattern->velocityAt (s, row.note))
            {
                const float strength = 0.35f + 0.65f * (float) velocity / 127.0f;
                g.setColour (colours::accent.withAlpha (strength));
                g.fillRoundedRectangle (cell, 2.0f);
            }
        }
    }

    // Bar lines
    g.setColour (colours::text.withAlpha (0.35f));
    for (int bar = 1; bar < pattern->bars; ++bar)
    {
        const float x = (float) labelWidth + (float) (bar * pattern->stepsPerBar) * cellWidth;
        g.drawLine (x, 0.0f, x, (float) getHeight(), 1.5f);
    }

    if (playStep >= 0 && playStep < total)
    {
        g.setColour (colours::text.withAlpha (0.18f));
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
} // namespace woody
