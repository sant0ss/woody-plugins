#include "EqParams.h"
#include "WoodySaturation.h"

namespace eqwoody
{
juce::String bandId (int band, const char* name)
{
    return "b" + juce::String (band + 1) + "_" + name;
}

float defaultFrequency (int band)
{
    // Spread the bands over the spectrum so a new band lands somewhere useful
    return 40.0f * std::pow (350.0f, (float) band / (float) (numBands - 1));
}

juce::StringArray typeNames()
{
    return { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf" };
}

juce::StringArray placementNames()
{
    return { "Stereo", "Left", "Right", "Mid", "Side" };
}

juce::StringArray slopeNames()
{
    return { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct" };
}

juce::StringArray qualityNames()
{
    return { "Low", "Medium", "High", "Max" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;

    auto freqText = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
    auto gainText = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto qText = [] (float v, int) { return juce::String (v, 2); };
    auto percentText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int b = 0; b < numBands; ++b)
    {
        auto id = [b] (const char* name) { return juce::ParameterID { bandId (b, name), 1 }; };
        const juce::String n = "Band " + juce::String (b + 1) + " ";

        Range freqRange (20.0f, 20000.0f, 0.0f);
        freqRange.setSkewForCentre (1000.0f);
        Range qRange (0.1f, 40.0f, 0.0f);
        qRange.setSkewForCentre (1.5f);

        layout.add (std::make_unique<juce::AudioParameterBool> (id ("on"), n + "On", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (id ("type"), n + "Type", typeNames(), (int) Bell));
        layout.add (std::make_unique<Float> (id ("freq"), n + "Frequency", freqRange, defaultFrequency (b),
                                             Attributes().withStringFromValueFunction (freqText)));
        layout.add (std::make_unique<Float> (id ("gain"), n + "Gain", Range (-30.0f, 30.0f, 0.0f), 0.0f,
                                             Attributes().withStringFromValueFunction (gainText)));
        layout.add (std::make_unique<Float> (id ("q"), n + "Q", qRange, 1.0f,
                                             Attributes().withStringFromValueFunction (qText)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (id ("slope"), n + "Slope", slopeNames(), 1));
        layout.add (std::make_unique<juce::AudioParameterChoice> (id ("place"), n + "Placement", placementNames(), (int) Stereo));
        layout.add (std::make_unique<Float> (id ("sat"), n + "Saturation", Range (0.0f, 1.0f, 0.0f), 0.0f,
                                             Attributes().withStringFromValueFunction (percentText)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (id ("satmode"), n + "Saturation Mode",
                                                                  juce::StringArray::fromTokens (woody::satModeChoices, "|", ""), 0));
    }

    layout.add (std::make_unique<Float> (juce::ParameterID { "out", 1 }, "Output", Range (-24.0f, 24.0f, 0.0f), 0.0f,
                                         Attributes().withStringFromValueFunction (gainText)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "phase", 1 }, "Phase Mode",
                                                              juce::StringArray { "Zero Latency", "Linear Phase" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quality", 1 }, "Linear Phase Quality", qualityNames(), 2));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "analyzer", 1 }, "Analyzer",
                                                              juce::StringArray { "Off", "Post", "Pre", "Pre + Post" }, 3));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "range", 1 }, "Display Range",
                                                              juce::StringArray { "6 dB", "12 dB", "24 dB", "30 dB" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}
} // namespace eqwoody
