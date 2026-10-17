#include "Effects.h"
#include "Dsp.h"
#include <cmath>

namespace synthwoody
{
namespace
{
constexpr float maxDelaySeconds = 1.2f;

float softLimit (float x)
{
    const float a = std::abs (x);
    if (a <= 0.9f)
        return x;

    const float limited = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
    return x < 0.0f ? -limited : limited;
}
} // namespace

void Effects::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
    chorus.prepare (spec);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.0f);

    reverb.setSampleRate (sampleRate);

    const auto size = (size_t) (maxDelaySeconds * (float) sampleRate) + 4;
    for (auto& line : delayLine)
        line.assign (size, 0.0f);

    reset();
}

void Effects::reset()
{
    chorus.reset();
    reverb.reset();

    for (auto& line : delayLine)
        std::fill (line.begin(), line.end(), 0.0f);

    writePosition = 0;
    feedbackLowpass = { 0.0f, 0.0f };
    smoothedDelay = 0.0f;
}

void Effects::process (juce::AudioBuffer<float>& buffer, const FxSettings& fx)
{
    const int numSamples = buffer.getNumSamples();
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    // Drive: saturation with the level kept roughly constant
    if (fx.drive > 0.01f)
    {
        const float gain = 1.0f + fx.drive * 7.0f;
        const float makeup = 1.0f / std::sqrt (gain);
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = dsp::fastTanh (left[i] * gain) * makeup * 1.2f;
            right[i] = dsp::fastTanh (right[i] * gain) * makeup * 1.2f;
        }
    }

    // Chorus
    if (fx.chorusMix > 0.005f)
    {
        chorus.setRate (fx.chorusRate);
        chorus.setDepth (fx.chorusDepth);
        chorus.setMix (fx.chorusMix);

        float* channels[2] = { left, right };
        juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> context (block);
        chorus.process (context);
    }

    // Ping-pong delay with a darkening feedback path
    if (fx.delayMix > 0.005f)
    {
        const int lineLength = (int) delayLine[0].size();
        const float targetDelay = juce::jlimit (1.0f, (float) lineLength - 3.0f, fx.delayMs * 0.001f * (float) sampleRate);
        if (smoothedDelay == 0.0f)
            smoothedDelay = targetDelay;

        const float lowpass = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4500.0f / (float) sampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            smoothedDelay += (targetDelay - smoothedDelay) * 0.0008f;

            float readPosition = (float) writePosition - smoothedDelay;
            if (readPosition < 0.0f)
                readPosition += (float) lineLength;

            const int index = (int) readPosition;
            const float frac = readPosition - (float) index;
            const int next = index + 1 < lineLength ? index + 1 : 0;

            const float delayedL = delayLine[0][(size_t) index] + frac * (delayLine[0][(size_t) next] - delayLine[0][(size_t) index]);
            const float delayedR = delayLine[1][(size_t) index] + frac * (delayLine[1][(size_t) next] - delayLine[1][(size_t) index]);

            feedbackLowpass[0] += (delayedR - feedbackLowpass[0]) * lowpass; // cross-fed for the ping-pong
            feedbackLowpass[1] += (delayedL - feedbackLowpass[1]) * lowpass;

            delayLine[0][(size_t) writePosition] = left[i] + feedbackLowpass[0] * fx.delayFeedback;
            delayLine[1][(size_t) writePosition] = right[i] + feedbackLowpass[1] * fx.delayFeedback;

            left[i] += delayedL * fx.delayMix;
            right[i] += delayedR * fx.delayMix;

            if (++writePosition >= lineLength)
                writePosition = 0;
        }
    }

    // Reverb (blended in by hand to keep the dry signal at full level)
    if (fx.reverbMix > 0.005f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = fx.reverbSize;
        rp.damping = 0.5f;
        rp.wetLevel = fx.reverbMix * 0.5f;
        rp.dryLevel = 1.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
        reverb.processStereo (left, right, numSamples);
    }

    // Master
    const float master = juce::Decibels::decibelsToGain (fx.masterDb, -60.0f);
    buffer.applyGainRamp (0, numSamples, lastMaster, master);
    lastMaster = master;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = softLimit (left[i]);
        right[i] = softLimit (right[i]);
    }
}
} // namespace synthwoody
