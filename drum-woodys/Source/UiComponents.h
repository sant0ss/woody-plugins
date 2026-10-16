#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Patterns.h"
#include "WoodyLookAndFeel.h"

namespace woody
{
// A rotary knob with a caption, attached to a parameter.
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
                 juce::Colour colour = colours::accent);
    void resized() override;
    void setTooltip (const juce::String& text) { slider.setTooltip (text); }

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Drag it into the DAW to drop the MIDI of a groove or song as a clip.
class MidiDragButton : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    explicit MidiDragButton (const juce::String& text, bool compact = false);

    std::function<Sequence()> getSequence;
    std::function<juce::String()> getName;
    std::function<float()> getTiming;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String text;
    bool compact;
    bool dragging = false;
};

// Step grid: one row per drum articulation, one column per step.
// Editable in the Editor tab, read-only as a preview in the Grooves tab.
class StepGrid : public juce::Component
{
public:
    StepGrid();

    void setPattern (Pattern* p, bool editable);
    void setCompact (bool onlyUsedRows); // preview shows only rows the pattern uses
    void setPlayStep (int step);          // -1 hides the playhead
    void refresh();

    int getIdealHeight() const;

    std::function<void()> onEditStart;   // before a click changes the pattern (for undo)
    std::function<void()> onChange;
    std::function<void (int note, float velocity)> onAudition;

    static constexpr int labelWidth = 120;
    static constexpr int rowHeight = 20;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    struct Cell
    {
        int step = -1;
        int row = -1;
    };

    Cell cellAt (juce::Point<int>) const;
    void applyPaint (Cell);

    Pattern* pattern = nullptr;
    bool editable = false;
    bool compact = false;
    int playStep = -1;
    std::vector<DrumRow> rows;

    bool paintAdds = true;
    Cell lastPainted;
};

// Vertical peak meter with a falling bar and a short peak hold.
class LevelMeter : public juce::Component
{
public:
    void setLevel (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, hold = 0.0f;
    int holdTicks = 0;
};

// Segmented pill control used for the main tabs.
class PillTabs : public juce::Component
{
public:
    explicit PillTabs (const juce::StringArray& names);

    std::function<void (int)> onChange;
    void setSelected (int index, bool notify = true);
    int getSelected() const { return selected; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray names;
    int selected = 0;
};
} // namespace woody
