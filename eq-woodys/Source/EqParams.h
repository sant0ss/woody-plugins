#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace eqwoody
{
constexpr int numBands = 16;

enum BandType { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, Tilt, numTypes };
enum Placement { Stereo, Left, Right, Mid, Side };

// Every band has these parameters, named "b<1..16>_<name>".
struct BandSettings
{
    bool on = false;
    int type = Bell;
    float freq = 1000.0f;
    float gain = 0.0f;
    float q = 1.0f;
    int slope = 1;      // index into slopeNames(): 6, 12, 18, 24, 36, 48, 72, 96 dB/oct
    int place = Stereo;
    float sat = 0.0f;   // band saturation amount 0..1
    int satMode = 0;
};

using BandsArray = std::array<BandSettings, numBands>;

juce::String bandId (int band, const char* name);
float defaultFrequency (int band);

juce::StringArray typeNames();
juce::StringArray placementNames();
juce::StringArray slopeNames();
juce::StringArray qualityNames();

inline bool isCut (int type) { return type == LowCut || type == HighCut; }
inline bool hasGain (int type) { return type == Bell || type == LowShelf || type == HighShelf || type == Tilt; }

// True when the band changes the sound at all.
inline bool isActive (const BandSettings& b) { return b.on && (! hasGain (b.type) || std::abs (b.gain) >= 0.01f); }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace eqwoody
