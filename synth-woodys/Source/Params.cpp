#include "Params.h"

namespace synthwoody
{
const std::array<ParamDef, numParams>& paramDefs()
{
    static const std::array<ParamDef, numParams> defs {{
#define X(id, name, kind, mn, mx, df, ce, un, ch) ParamDef { #id, name, Kind::kind, mn, mx, df, ce, un, ch },
        SW_PARAM_LIST (X)
#undef X
    }};
    return defs;
}

juce::StringArray choicesOf (ParamIndex index)
{
    return juce::StringArray::fromTokens (juce::String::fromUTF8 (paramDefs()[(size_t) index].choices), "|", "");
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < numParams; ++i)
    {
        const auto& d = paramDefs()[(size_t) i];
        const juce::ParameterID id { d.id, 1 };

        switch (d.kind)
        {
            case Kind::Float:
            {
                juce::NormalisableRange<float> range (d.min, d.max, 0.0f);
                if (d.centre > d.min && d.centre < d.max)
                    range.setSkewForCentre (d.centre);

                layout.add (std::make_unique<juce::AudioParameterFloat> (id, d.name, range, d.def,
                                                                         juce::AudioParameterFloatAttributes().withLabel (d.unit)));
                break;
            }
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (id, d.name, (int) d.min, (int) d.max, (int) d.def,
                                                                       juce::AudioParameterIntAttributes().withLabel (d.unit)));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, d.name, choicesOf ((ParamIndex) i), (int) d.def));
                break;
        }
    }

    return layout;
}

// Values are in real units; choices are their index. Anything not listed keeps its default.
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Init Saw", {} },

        { "Warm Pad",
          { { "aShape", 0 }, { "aLevel", 0.5f }, { "unison", 5 }, { "uniDetune", 0.45f }, { "uniSpread", 0.8f },
            { "bTable", 3 }, { "bPos", 0.2f }, { "bLevel", 0.4f },
            { "cutoff", 1800 }, { "res", 0.15f }, { "envAmt", 0.25f },
            { "ampA", 0.9f }, { "ampD", 1.0f }, { "ampS", 0.8f }, { "ampR", 1.8f },
            { "fltA", 0.8f }, { "fltD", 1.5f }, { "fltS", 0.4f }, { "fltR", 1.5f },
            { "lfo1Rate", 0.2f }, { "m1Src", 1 }, { "m1Dst", 3 }, { "m1Amt", 0.3f },
            { "chMix", 0.4f }, { "dlMix", 0.15f }, { "rvMix", 0.35f }, { "rvSize", 0.7f } } },

        { "Detuned Supersaw",
          { { "aShape", 0 }, { "aLevel", 0.7f }, { "bLevel", 0.0f }, { "unison", 7 }, { "uniDetune", 0.6f }, { "uniSpread", 1.0f },
            { "cutoff", 6500 }, { "res", 0.1f }, { "envAmt", 0.2f },
            { "ampA", 0.01f }, { "ampS", 0.8f }, { "ampR", 0.5f },
            { "chMix", 0.3f }, { "rvMix", 0.2f } } },

        { "Sub Bass",
          { { "aShape", 3 }, { "aLevel", 0.8f }, { "bLevel", 0.0f }, { "subLevel", 0.6f },
            { "cutoff", 900 }, { "res", 0.1f }, { "fDrive", 0.3f }, { "envAmt", 0.2f },
            { "ampA", 0.002f }, { "ampD", 0.3f }, { "ampS", 0.9f }, { "ampR", 0.15f },
            { "rvMix", 0.0f } } },

        { "Wobble Bass",
          { { "aShape", 0 }, { "aLevel", 0.7f }, { "bTable", 5 }, { "bPos", 0.5f }, { "bLevel", 0.5f },
            { "unison", 3 }, { "uniDetune", 0.15f }, { "subLevel", 0.3f },
            { "cutoff", 500 }, { "res", 0.45f }, { "fDrive", 0.4f }, { "envAmt", 0.1f },
            { "ampS", 0.9f }, { "ampR", 0.2f },
            { "lfo1Shape", 1 }, { "lfo1Rate", 2.5f }, { "m1Src", 1 }, { "m1Dst", 0 }, { "m1Amt", 0.6f },
            { "rvMix", 0.05f } } },

        { "Pluck",
          { { "aShape", 0 }, { "aLevel", 0.6f }, { "bTable", 1 }, { "bPos", 0.3f }, { "bLevel", 0.4f },
            { "unison", 3 }, { "uniDetune", 0.2f },
            { "cutoff", 700 }, { "res", 0.25f }, { "envAmt", 0.7f },
            { "ampA", 0.001f }, { "ampD", 0.3f }, { "ampS", 0.0f }, { "ampR", 0.25f },
            { "fltA", 0.001f }, { "fltD", 0.22f }, { "fltS", 0.0f },
            { "dlMix", 0.25f }, { "rvMix", 0.2f } } },

        { "Glass Keys",
          { { "aLevel", 0.0f }, { "bTable", 4 }, { "bPos", 0.4f }, { "bLevel", 0.7f },
            { "unison", 2 }, { "uniDetune", 0.1f },
            { "cutoff", 6000 }, { "envAmt", 0.0f },
            { "ampA", 0.003f }, { "ampD", 1.2f }, { "ampS", 0.2f }, { "ampR", 1.0f },
            { "fltD", 1.0f }, { "m1Src", 3 }, { "m1Dst", 3 }, { "m1Amt", 0.6f },
            { "chMix", 0.3f }, { "rvMix", 0.4f }, { "rvSize", 0.7f } } },

        { "Vocal Lead",
          { { "aLevel", 0.2f }, { "bTable", 2 }, { "bLevel", 0.8f }, { "unison", 3 }, { "uniDetune", 0.2f }, { "uniSpread", 0.4f },
            { "cutoff", 5000 }, { "envAmt", 0.1f },
            { "ampA", 0.05f }, { "ampS", 0.9f }, { "ampR", 0.35f },
            { "lfo1Rate", 0.4f }, { "m1Src", 1 }, { "m1Dst", 3 }, { "m1Amt", 0.5f },
            { "lfo2Rate", 5.5f }, { "m2Src", 2 }, { "m2Dst", 2 }, { "m2Amt", 0.03f },
            { "dlMix", 0.2f }, { "rvMix", 0.25f } } },

        { "Noise Sweep",
          { { "aLevel", 0.0f }, { "bLevel", 0.0f }, { "noiseLevel", 0.6f },
            { "fType", 2 }, { "cutoff", 1200 }, { "res", 0.6f }, { "envAmt", 0.0f },
            { "ampA", 1.2f }, { "ampD", 1.0f }, { "ampS", 1.0f }, { "ampR", 2.0f },
            { "lfo1Rate", 0.3f }, { "m1Src", 1 }, { "m1Dst", 0 }, { "m1Amt", 0.8f },
            { "rvMix", 0.5f }, { "rvSize", 0.8f } } },
    };

    return presets;
}
} // namespace synthwoody
