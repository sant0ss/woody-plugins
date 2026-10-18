#include "SatParams.h"
#include "WoodySaturation.h"
#include "WoodyTheme.h"

namespace satwoody
{
juce::StringArray typeNames()
{
    return { woody::utf8 ("Suave"), woody::utf8 ("Válvula"), "Fita", "Duro", "Dobra" };
}

juce::StringArray stereoNames()
{
    return { woody::utf8 ("Estéreo"), "Mid (centro)", "Side (laterais)" };
}

juce::StringArray oversamplingNames()
{
    return { "Off", "2x", "4x", "8x" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;

    auto id = [] (const char* name) { return juce::ParameterID { name, 1 }; };
    auto gainText = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto percentText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto toneText = [] (float v, int)
    {
        const int percent = juce::roundToInt (v * 100.0f);
        return percent == 0 ? juce::String ("neutro") : (percent > 0 ? "+" : "") + juce::String (percent) + " %";
    };
    auto freqText = [] (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    };

    Range lowRange (focusLowMin, focusLowMax, 0.0f);
    lowRange.setSkewForCentre (200.0f);
    Range highRange (focusHighMin, focusHighMax, 0.0f);
    highRange.setSkewForCentre (4000.0f);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Float> (id ("in"), "Input", Range (-24.0f, 24.0f, 0.0f), 0.0f,
                                         Attributes().withStringFromValueFunction (gainText)));
    layout.add (std::make_unique<Float> (id ("drive"), "Drive", Range (0.0f, 1.0f, 0.0f), 0.3f,
                                         Attributes().withStringFromValueFunction (percentText)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("type"), "Type", typeNames(), 0));
    layout.add (std::make_unique<Float> (id ("tone"), "Tone", Range (-1.0f, 1.0f, 0.0f), 0.0f,
                                         Attributes().withStringFromValueFunction (toneText)));
    layout.add (std::make_unique<Float> (id ("bias"), "Bias", Range (-1.0f, 1.0f, 0.0f), 0.0f,
                                         Attributes().withStringFromValueFunction (toneText)));
    layout.add (std::make_unique<Float> (id ("mix"), "Mix", Range (0.0f, 1.0f, 0.0f), 1.0f,
                                         Attributes().withStringFromValueFunction (percentText)));
    layout.add (std::make_unique<Float> (id ("out"), "Output", Range (-24.0f, 12.0f, 0.0f), 0.0f,
                                         Attributes().withStringFromValueFunction (gainText)));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("auto"), "Auto Gain", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("stereo"), "Stereo Mode", stereoNames(), (int) Stereo));
    layout.add (std::make_unique<Float> (id ("focuslow"), "Focus Low Cut", lowRange, focusLowMin,
                                         Attributes().withStringFromValueFunction (freqText)));
    layout.add (std::make_unique<Float> (id ("focushigh"), "Focus High Cut", highRange, focusHighMax,
                                         Attributes().withStringFromValueFunction (freqText)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("os"), "Oversampling", oversamplingNames(), 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("bypass"), "Bypass", false));

    return layout;
}
} // namespace satwoody
