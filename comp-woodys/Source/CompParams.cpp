#include "CompParams.h"
#include "WoodyTheme.h"

namespace compwoody
{
namespace
{
using Float = juce::AudioParameterFloat;
using Attributes = juce::AudioParameterFloatAttributes;
using Range = juce::NormalisableRange<float>;

constexpr float ratioKnobTravel = 0.95f; // share of the knob that covers 1:1 to 20:1
constexpr float ratioCurve = 2.5f;
constexpr float ratioSpan = 19.0f; // 20:1 minus 1:1

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi, 0.0f);
    r.setSkewForCentre (centre);
    return r;
}

juce::String dbText (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String msText (float v) { return v < 10.0f ? juce::String (v, 2) + " ms" : (v < 1000.0f ? juce::String (v, 0) + " ms" : juce::String (v / 1000.0f, 2) + " s"); }
juce::String hzText (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }

void addBandParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, int band)
{
    auto id = [band] (const char* name) { return juce::ParameterID { bandId (band, name), 1 }; };
    const juce::String n = "Band " + juce::String (band + 1) + " ";

    layout.add (std::make_unique<Float> (id ("thr"), n + "Threshold", Range (-60.0f, 0.0f, 0.1f), -24.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return dbText (v); })));
    layout.add (std::make_unique<Float> (id ("ratio"), n + "Ratio", ratioRange(), 3.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return ratioText (v); })));
    layout.add (std::make_unique<Float> (id ("att"), n + "Attack", skewed (0.1f, 200.0f, 10.0f), 20.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return msText (v); })));
    layout.add (std::make_unique<Float> (id ("rel"), n + "Release", skewed (5.0f, 2000.0f, 150.0f), 200.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return msText (v); })));
    layout.add (std::make_unique<Float> (id ("makeup"), n + "Makeup", Range (-12.0f, 24.0f, 0.1f), 0.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return dbText (v); })));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("solo"), n + "Solo", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("bypass"), n + "Bypass", false));
}
} // namespace

juce::String bandId (int band, const char* name)
{
    return "b" + juce::String (band + 1) + "_" + name;
}

juce::NormalisableRange<float> ratioRange()
{
    return Range (
        1.0f, limitRatio,
        [] (float, float, float n)
        {
            if (n > 0.96f)
                return limitRatio;
            return 1.0f + ratioSpan * std::pow (juce::jmin (n / ratioKnobTravel, 1.0f), ratioCurve);
        },
        [] (float, float, float v)
        {
            if (v >= limitThreshold)
                return 1.0f;
            return ratioKnobTravel * std::pow (juce::jlimit (0.0f, 1.0f, (v - 1.0f) / ratioSpan), 1.0f / ratioCurve);
        },
        [] (float, float, float v) { return v >= limitThreshold ? limitRatio : juce::jlimit (1.0f, 20.0f, std::round (v * 10.0f) / 10.0f); });
}

juce::String ratioText (float ratio)
{
    return ratio >= limitThreshold ? woody::utf8 ("Limite") : juce::String (ratio, 1) + ":1";
}

juce::StringArray lookaheadNames()
{
    return { "Off", "1 ms", "2 ms", "5 ms" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto id = [] (const char* name) { return juce::ParameterID { name, 1 }; };

    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("mode"), "Mode", juce::StringArray { "Single band", "Multiband" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("bypass"), "Bypass", false));

    layout.add (std::make_unique<Float> (id ("threshold"), "Threshold", Range (-60.0f, 0.0f, 0.1f), -18.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return dbText (v); })));
    layout.add (std::make_unique<Float> (id ("ratio"), "Ratio", ratioRange(), 4.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return ratioText (v); })));
    layout.add (std::make_unique<Float> (id ("attack"), "Attack", skewed (0.1f, 200.0f, 10.0f), 10.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return msText (v); })));
    layout.add (std::make_unique<Float> (id ("release"), "Release", skewed (5.0f, 2000.0f, 150.0f), 120.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return msText (v); })));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("autorelease"), "Auto Release", false));
    layout.add (std::make_unique<Float> (id ("knee"), "Knee", Range (0.0f, 24.0f, 0.1f), 6.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));
    layout.add (std::make_unique<Float> (id ("makeup"), "Makeup", Range (-12.0f, 24.0f, 0.1f), 0.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return dbText (v); })));
    layout.add (std::make_unique<juce::AudioParameterBool> (id ("automakeup"), "Auto Makeup", false));
    layout.add (std::make_unique<Float> (id ("mix"), "Mix", Range (0.0f, 100.0f, 1.0f), 100.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));
    layout.add (std::make_unique<Float> (id ("schpf"), "Sidechain High-pass", skewed (20.0f, 500.0f, 80.0f), 20.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? juce::String ("Off") : hzText (v); })));
    layout.add (std::make_unique<Float> (id ("link"), "Stereo Link", Range (0.0f, 100.0f, 1.0f), 100.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("detector"), "Detector", juce::StringArray { "Peak", "RMS" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("lookahead"), "Lookahead", lookaheadNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("channel"), "Channel Mode", juce::StringArray { "Stereo", "Mid-Side" }, 0));

    const float defaults[numCrossovers] = { 120.0f, 1000.0f, 5000.0f };
    for (int i = 0; i < numCrossovers; ++i)
        layout.add (std::make_unique<Float> (juce::ParameterID { "x" + juce::String (i + 1), 1 }, "Crossover " + juce::String (i + 1),
                                             skewed (minCrossoverFreq, 20000.0f, 1000.0f), defaults[i],
                                             Attributes().withStringFromValueFunction ([] (float v, int) { return hzText (v); })));

    for (int b = 0; b < numBands; ++b)
        addBandParameters (layout, b);

    return layout;
}

ParamSet::ParamSet (juce::AudioProcessorValueTreeState& state)
{
    auto get = [&state] (const juce::String& name) { return state.getRawParameterValue (name); };

    mode = get ("mode");
    bypass = get ("bypass");
    threshold = get ("threshold");
    ratio = get ("ratio");
    attack = get ("attack");
    release = get ("release");
    autoRelease = get ("autorelease");
    knee = get ("knee");
    makeup = get ("makeup");
    autoMakeup = get ("automakeup");
    mix = get ("mix");
    scHighPass = get ("schpf");
    link = get ("link");
    detector = get ("detector");
    lookahead = get ("lookahead");
    channel = get ("channel");

    for (int i = 0; i < numCrossovers; ++i)
        crossovers[(size_t) i] = get ("x" + juce::String (i + 1));

    for (int b = 0; b < numBands; ++b)
        bands[(size_t) b] = { get (bandId (b, "thr")),    get (bandId (b, "ratio")),  get (bandId (b, "att")),   get (bandId (b, "rel")),
                              get (bandId (b, "makeup")), get (bandId (b, "solo")),   get (bandId (b, "bypass")) };
}

EngineParams ParamSet::read() const
{
    EngineParams p;
    p.multiband = mode->load() > 0.5f;
    p.bypass = bypass->load() > 0.5f;
    p.midSide = channel->load() > 0.5f;
    p.lookaheadChoice = (int) lookahead->load();

    auto& s = p.single;
    s.thresholdDb = threshold->load();
    s.ratio = ratio->load();
    s.attackMs = attack->load();
    s.releaseMs = release->load();
    s.autoRelease = autoRelease->load() > 0.5f;
    s.kneeDb = knee->load();
    s.makeupDb = makeup->load();
    s.autoMakeup = autoMakeup->load() > 0.5f;
    s.mix = mix->load() * 0.01f;
    s.scHighPassHz = scHighPass->load();
    s.link = link->load() * 0.01f;
    s.rms = detector->load() > 0.5f;

    for (int i = 0; i < numCrossovers; ++i)
        p.crossovers[(size_t) i] = crossovers[(size_t) i]->load();

    for (int b = 0; b < numBands; ++b)
    {
        const auto& src = bands[(size_t) b];
        auto& dst = p.bands[(size_t) b];
        dst.thresholdDb = src.threshold->load();
        dst.ratio = src.ratio->load();
        dst.attackMs = src.attack->load();
        dst.releaseMs = src.release->load();
        dst.makeupDb = src.makeup->load();
        dst.solo = src.solo->load() > 0.5f;
        dst.bypass = src.bypass->load() > 0.5f;
    }

    return p;
}
} // namespace compwoody
