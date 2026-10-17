#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "WoodyTheme.h"

namespace synthwoody
{
// A rotary knob with a caption, attached to a parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, ParamIndex, const juce::String& caption, juce::Colour);
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Base for the small displays that redraw from the parameter values.
class ParamView : public juce::Component
{
public:
    explicit ParamView (juce::AudioProcessorValueTreeState& s) : state (s) {}

protected:
    float value (ParamIndex i) const { return state.getRawParameterValue (paramDefs()[(size_t) i].id)->load(); }
    juce::AudioProcessorValueTreeState& state;
};

// Oscillator A: the analog shape.
class AnalogWaveView : public ParamView
{
public:
    using ParamView::ParamView;
    void paint (juce::Graphics&) override;
};

// Oscillator B: the wavetable frame at the current position.
class WavetableView : public ParamView
{
public:
    using ParamView::ParamView;
    void paint (juce::Graphics&) override;
};

// ADSR curve.
class EnvelopeView : public ParamView
{
public:
    EnvelopeView (juce::AudioProcessorValueTreeState&, ParamIndex a, ParamIndex d, ParamIndex s, ParamIndex r, juce::Colour);
    void paint (juce::Graphics&) override;

private:
    ParamIndex pa, pd, ps, pr;
    juce::Colour colour;
};

// Approximate magnitude response of the selected filter.
class FilterView : public ParamView
{
public:
    using ParamView::ParamView;
    void paint (juce::Graphics&) override;
};

void paintDisplayBackground (juce::Graphics&, juce::Rectangle<float>);
} // namespace synthwoody
