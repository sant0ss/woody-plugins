#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WoodyTheme.h"

namespace woody
{
// A rotary knob with a caption, attached to a parameter by id.
class ParamKnob : public juce::Component
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
               juce::Colour colour = colours::accent);

    void resized() override;
    void setCaption (const juce::String&);

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// A horizontal slider with a caption on the left and the value on the right.
class ParamBar : public juce::Component
{
public:
    ParamBar (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
              juce::Colour colour = colours::accent);

    void resized() override;

    juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

private:
    juce::Label caption, value;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Peak meter bar; horizontal or vertical.
class PeakBar : public juce::Component
{
public:
    explicit PeakBar (bool isVertical = false) : vertical (isVertical) {}

    void setLevel (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    bool vertical;
    float level = 0.0f, hold = 0.0f;
    int holdTicks = 0;
};
} // namespace woody
