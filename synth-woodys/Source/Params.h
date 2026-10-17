#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace synthwoody
{
// id, display name, kind, min, max, default, centre (skew, 0 = linear), unit, choices ("a|b|c")
#define SW_PARAM_LIST(X) \
    X(aShape,    "Osc A Shape",        Choice, 0.0f,    3.0f,     0.0f,    0.0f,    "",   "Saw|Square|Triangle|Sine") \
    X(aPw,       "Osc A Pulse Width",  Float,  0.05f,   0.95f,    0.5f,    0.0f,    "",   "") \
    X(aSemi,     "Osc A Semitone",     Int,    -24.0f,  24.0f,    0.0f,    0.0f,    "st", "") \
    X(aFine,     "Osc A Fine",         Float,  -100.0f, 100.0f,   0.0f,    0.0f,    "ct", "") \
    X(aLevel,    "Osc A Level",        Float,  0.0f,    1.0f,     0.7f,    0.0f,    "",   "") \
    X(bTable,    "Osc B Wavetable",    Choice, 0.0f,    5.0f,     0.0f,    0.0f,    "",   "Basic|Digital|Vocal|Organ|Glass|Growl") \
    X(bPos,      "Osc B Position",     Float,  0.0f,    1.0f,     0.0f,    0.0f,    "",   "") \
    X(bSemi,     "Osc B Semitone",     Int,    -24.0f,  24.0f,    0.0f,    0.0f,    "st", "") \
    X(bFine,     "Osc B Fine",         Float,  -100.0f, 100.0f,   0.0f,    0.0f,    "ct", "") \
    X(bLevel,    "Osc B Level",        Float,  0.0f,    1.0f,     0.5f,    0.0f,    "",   "") \
    X(unison,    "Unison Voices",      Int,    1.0f,    7.0f,     1.0f,    0.0f,    "",   "") \
    X(uniDetune, "Unison Detune",      Float,  0.0f,    1.0f,     0.25f,   0.0f,    "",   "") \
    X(uniSpread, "Unison Spread",      Float,  0.0f,    1.0f,     0.5f,    0.0f,    "",   "") \
    X(subLevel,  "Sub Level",          Float,  0.0f,    1.0f,     0.0f,    0.0f,    "",   "") \
    X(noiseLevel,"Noise Level",        Float,  0.0f,    1.0f,     0.0f,    0.0f,    "",   "") \
    X(fType,     "Filter Type",        Choice, 0.0f,    3.0f,     0.0f,    0.0f,    "",   "Low Pass 24|Low Pass 12|Band Pass|High Pass") \
    X(cutoff,    "Filter Cutoff",      Float,  20.0f,   20000.0f, 3000.0f, 1000.0f, "Hz", "") \
    X(res,       "Filter Resonance",   Float,  0.0f,    1.0f,     0.2f,    0.0f,    "",   "") \
    X(fDrive,    "Filter Drive",       Float,  0.0f,    1.0f,     0.1f,    0.0f,    "",   "") \
    X(envAmt,    "Filter Env Amount",  Float,  -1.0f,   1.0f,     0.3f,    0.0f,    "",   "") \
    X(keytrack,  "Filter Keytrack",    Float,  0.0f,    1.0f,     0.3f,    0.0f,    "",   "") \
    X(ampA,      "Amp Attack",         Float,  0.001f,  8.0f,     0.01f,   0.3f,    "s",  "") \
    X(ampD,      "Amp Decay",          Float,  0.001f,  8.0f,     0.4f,    0.5f,    "s",  "") \
    X(ampS,      "Amp Sustain",        Float,  0.0f,    1.0f,     0.7f,    0.0f,    "",   "") \
    X(ampR,      "Amp Release",        Float,  0.001f,  10.0f,    0.3f,    0.5f,    "s",  "") \
    X(fltA,      "Filter Attack",      Float,  0.001f,  8.0f,     0.01f,   0.3f,    "s",  "") \
    X(fltD,      "Filter Decay",       Float,  0.001f,  8.0f,     0.5f,    0.5f,    "s",  "") \
    X(fltS,      "Filter Sustain",     Float,  0.0f,    1.0f,     0.3f,    0.0f,    "",   "") \
    X(fltR,      "Filter Release",     Float,  0.001f,  10.0f,    0.4f,    0.5f,    "s",  "") \
    X(lfo1Shape, "LFO 1 Shape",        Choice, 0.0f,    4.0f,     0.0f,    0.0f,    "",   "Sine|Triangle|Saw|Square|Random") \
    X(lfo1Rate,  "LFO 1 Rate",         Float,  0.05f,   30.0f,    2.0f,    2.0f,    "Hz", "") \
    X(lfo2Shape, "LFO 2 Shape",        Choice, 0.0f,    4.0f,     0.0f,    0.0f,    "",   "Sine|Triangle|Saw|Square|Random") \
    X(lfo2Rate,  "LFO 2 Rate",         Float,  0.05f,   30.0f,    0.5f,    2.0f,    "Hz", "") \
    X(m1Src,     "Mod 1 Source",       Choice, 0.0f,    5.0f,     0.0f,    0.0f,    "",   "None|LFO 1|LFO 2|Filter Env|Velocity|Mod Wheel") \
    X(m1Dst,     "Mod 1 Destination",  Choice, 0.0f,    7.0f,     0.0f,    0.0f,    "",   "Cutoff|Resonance|Pitch|WT Position|Pulse Width|Osc A Level|Osc B Level|Volume") \
    X(m1Amt,     "Mod 1 Amount",       Float,  -1.0f,   1.0f,     0.0f,    0.0f,    "",   "") \
    X(m2Src,     "Mod 2 Source",       Choice, 0.0f,    5.0f,     0.0f,    0.0f,    "",   "None|LFO 1|LFO 2|Filter Env|Velocity|Mod Wheel") \
    X(m2Dst,     "Mod 2 Destination",  Choice, 0.0f,    7.0f,     0.0f,    0.0f,    "",   "Cutoff|Resonance|Pitch|WT Position|Pulse Width|Osc A Level|Osc B Level|Volume") \
    X(m2Amt,     "Mod 2 Amount",       Float,  -1.0f,   1.0f,     0.0f,    0.0f,    "",   "") \
    X(m3Src,     "Mod 3 Source",       Choice, 0.0f,    5.0f,     0.0f,    0.0f,    "",   "None|LFO 1|LFO 2|Filter Env|Velocity|Mod Wheel") \
    X(m3Dst,     "Mod 3 Destination",  Choice, 0.0f,    7.0f,     0.0f,    0.0f,    "",   "Cutoff|Resonance|Pitch|WT Position|Pulse Width|Osc A Level|Osc B Level|Volume") \
    X(m3Amt,     "Mod 3 Amount",       Float,  -1.0f,   1.0f,     0.0f,    0.0f,    "",   "") \
    X(m4Src,     "Mod 4 Source",       Choice, 0.0f,    5.0f,     0.0f,    0.0f,    "",   "None|LFO 1|LFO 2|Filter Env|Velocity|Mod Wheel") \
    X(m4Dst,     "Mod 4 Destination",  Choice, 0.0f,    7.0f,     0.0f,    0.0f,    "",   "Cutoff|Resonance|Pitch|WT Position|Pulse Width|Osc A Level|Osc B Level|Volume") \
    X(m4Amt,     "Mod 4 Amount",       Float,  -1.0f,   1.0f,     0.0f,    0.0f,    "",   "") \
    X(fxDrive,   "FX Drive",           Float,  0.0f,    1.0f,     0.0f,    0.0f,    "",   "") \
    X(chMix,     "Chorus Mix",         Float,  0.0f,    1.0f,     0.0f,    0.0f,    "",   "") \
    X(chRate,    "Chorus Rate",        Float,  0.05f,   5.0f,     0.8f,    0.8f,    "Hz", "") \
    X(chDepth,   "Chorus Depth",       Float,  0.0f,    1.0f,     0.4f,    0.0f,    "",   "") \
    X(dlMix,     "Delay Mix",          Float,  0.0f,    1.0f,     0.0f,    0.0f,    "",   "") \
    X(dlTime,    "Delay Time",         Float,  20.0f,   1000.0f,  320.0f,  250.0f,  "ms", "") \
    X(dlFb,      "Delay Feedback",     Float,  0.0f,    0.95f,    0.35f,   0.0f,    "",   "") \
    X(rvMix,     "Reverb Mix",         Float,  0.0f,    1.0f,     0.15f,   0.0f,    "",   "") \
    X(rvSize,    "Reverb Size",        Float,  0.0f,    1.0f,     0.5f,    0.0f,    "",   "") \
    X(master,    "Master",             Float,  -30.0f,  6.0f,     -6.0f,   0.0f,    "dB", "")

enum ParamIndex
{
#define X(id, name, kind, mn, mx, df, ce, un, ch) id,
    SW_PARAM_LIST (X)
#undef X
    numParams
};

enum class Kind { Float, Int, Choice };

struct ParamDef
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def, centre;
    const char* unit;
    const char* choices;
};

const std::array<ParamDef, numParams>& paramDefs();
juce::StringArray choicesOf (ParamIndex);
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Modulation slots: slot i uses m1Src + 3 * i, +1 for the destination, +2 for the amount.
constexpr int numModSlots = 4;

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, double>> values; // parameter id -> value in real units
};

const std::vector<FactoryPreset>& factoryPresets();
} // namespace synthwoody
