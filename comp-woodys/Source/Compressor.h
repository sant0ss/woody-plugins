#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace compwoody
{
// Ratio values from limitThreshold up mean "limit" (infinite ratio); limitRatio is what the knob shows there
constexpr float limitRatio = 21.0f;
constexpr float limitThreshold = 20.5f;
constexpr float maxMakeupDb = 24.0f;

inline float inverseRatio (float ratio) { return ratio >= limitThreshold ? 0.0f : 1.0f / juce::jmax (1.0f, ratio); }

// Gain reduction in dB (>= 0) that a static soft-knee compressor applies to a level in dB
float gainReductionDb (float levelDb, float thresholdDb, float invRatio, float kneeDb);

// Makeup in dB that brings a full-scale signal back to full scale
float autoMakeupDb (float thresholdDb, float invRatio);

// Feed-forward compressor core for two channels, shared by the single-band mode and every multiband band.
// Level detection, the gain computer and the attack/release smoothing all work in dB.
class Compressor
{
public:
    struct Settings
    {
        float thresholdDb = -18.0f;
        float ratio = 4.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        float kneeDb = 6.0f;
        float makeupDb = 0.0f;
        float mix = 1.0f;
        float scHighPassHz = 20.0f;
        float link = 1.0f;
        bool autoRelease = false;
        bool autoMakeup = false;
        bool rms = false;
        int lookaheadSamples = 0;
    };

    void prepare (double sampleRate, int maxLookaheadSamples);
    void reset(); // the next setSettings call jumps instead of ramping
    void setSettings (const Settings&);

    // In place, any number of samples
    void process (float* left, float* right, int numSamples);

    float getGainReductionDb() const { return grMeter.load(); }
    float getInputLevelDb() const { return levelMeter.load(); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    void updateHighPass (float hz);

    static constexpr int numChannels = 2;

    double sampleRate = 48000.0;
    bool snapNext = true;

    Settings settings;
    float attackCoef = 0.0f, releaseCoef = 0.0f, peakDecay = 0.0f, rmsCoef = 0.0f;
    float avgUpCoef = 0.0f, avgDownCoef = 0.0f;
    float appliedHpf = -1.0f;
    bool hpfActive = false;
    Biquad hpf;
    std::array<std::array<double, 2>, numChannels> hpfState {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> threshold, invRatio, knee, mix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> makeupGain;

    std::array<float, numChannels> peak {}, meanSquare {}, grDb {}, avgGr {};

    std::array<std::vector<float>, numChannels> delayBuffer;
    int delayPos = 0;
    int latency = 0;

    std::atomic<float> grMeter { 0.0f }, levelMeter { -120.0f };
};
} // namespace compwoody
