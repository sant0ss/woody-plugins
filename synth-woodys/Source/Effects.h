#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace synthwoody
{
struct FxSettings
{
    float drive = 0.0f;
    float chorusMix = 0.0f, chorusRate = 0.8f, chorusDepth = 0.4f;
    float delayMix = 0.0f, delayMs = 320.0f, delayFeedback = 0.35f;
    float reverbMix = 0.15f, reverbSize = 0.5f;
    float masterDb = -6.0f;
};

// Drive -> chorus -> ping-pong delay -> reverb -> master with a soft limiter.
class Effects
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>&, const FxSettings&);

private:
    double sampleRate = 44100.0;

    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;

    std::array<std::vector<float>, 2> delayLine;
    int writePosition = 0;
    float smoothedDelay = 0.0f;
    std::array<float, 2> feedbackLowpass {};

    float lastMaster = 1.0f;
};
} // namespace synthwoody
