#include "WoodyWidgets.h"

namespace woody
{
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption,
                      juce::Colour colour)
    : attachment (state, paramId, slider)
{
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);

    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));

    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    label.setColour (juce::Label::textColourId, colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (label);
}

void ParamKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (15));
    slider.setBounds (area);
}

void ParamKnob::setCaption (const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
}

//==============================================================================
ParamBar::ParamBar (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text,
                    juce::Colour colour)
    : attachment (state, paramId, slider)
{
    slider.setColour (juce::Slider::trackColourId, colour);

    if (auto* p = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
        slider.onValueChange = [this, p]
        {
            value.setText (p->getText (p->convertTo0to1 ((float) slider.getValue()), 0), juce::dontSendNotification);
        };
    }

    addAndMakeVisible (slider);

    caption.setText (text, juce::dontSendNotification);
    caption.setFont (juce::FontOptions (12.0f));
    caption.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (caption);

    value.setFont (juce::FontOptions (12.0f));
    value.setJustificationType (juce::Justification::centredRight);
    value.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (value);

    if (slider.onValueChange != nullptr)
        slider.onValueChange();
}

void ParamBar::resized()
{
    auto area = getLocalBounds();
    caption.setBounds (area.removeFromLeft (juce::jmin (70, area.getWidth() / 3)));
    value.setBounds (area.removeFromRight (64));
    slider.setBounds (area);
}

//==============================================================================
void PeakBar::setLevel (float linearPeak)
{
    const float target = juce::jlimit (0.0f, 1.2f, linearPeak);
    level = target > level ? target : level * 0.88f;

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

void PeakBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (bounds, 3.0f);

    // -60 .. +3 dB across the bar
    auto proportion = [] (float linear)
    {
        const float db = juce::Decibels::gainToDecibels (linear, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f);
    };

    const float p = proportion (level);
    if (p > 0.0f)
    {
        juce::Rectangle<float> filled = vertical ? bounds.withTop (bounds.getBottom() - bounds.getHeight() * p)
                                                 : bounds.withWidth (bounds.getWidth() * p);

        juce::ColourGradient gradient (colours::accent2, vertical ? bounds.getBottomLeft() : bounds.getTopLeft(),
                                       colours::mute, vertical ? bounds.getTopLeft() : bounds.getTopRight(), false);
        gradient.addColour (0.8, colours::solo);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (filled, 3.0f);
    }

    if (hold > 0.001f)
    {
        const float h = proportion (hold);
        g.setColour (hold >= 1.0f ? colours::mute : colours::text.withAlpha (0.7f));
        if (vertical)
            g.fillRect (bounds.getX(), bounds.getBottom() - bounds.getHeight() * h - 1.0f, bounds.getWidth(), 2.0f);
        else
            g.fillRect (bounds.getX() + bounds.getWidth() * h - 1.0f, bounds.getY(), 2.0f, bounds.getHeight());
    }
}
} // namespace woody
