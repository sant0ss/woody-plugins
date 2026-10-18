#include "Compressor.h"
#include <cmath>

namespace compwoody
{
namespace
{
constexpr float dbToNepers = 0.11512925f; // ln(10) / 20
constexpr double pi = 3.14159265358979323846;

float coefficient (double timeMs, double sampleRate)
{
    return (float) std::exp (-1.0 / (juce::jmax (0.01, timeMs) * 0.001 * sampleRate));
}

float toDb (float linear)
{
    return 20.0f * std::log10 (juce::jmax (linear, 1.0e-6f));
}
} // namespace

float gainReductionDb (float levelDb, float thresholdDb, float invRatio, float kneeDb)
{
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f - invRatio;

    if (kneeDb < 0.01f)
        return over > 0.0f ? over * slope : 0.0f;

    if (2.0f * over < -kneeDb)
        return 0.0f;

    if (2.0f * std::abs (over) <= kneeDb)
    {
        const float x = over + kneeDb * 0.5f;
        return slope * x * x / (2.0f * kneeDb);
    }

    return over * slope;
}

float autoMakeupDb (float thresholdDb, float invRatio)
{
    return juce::jlimit (0.0f, maxMakeupDb, -thresholdDb * (1.0f - invRatio));
}

void Compressor::prepare (double newSampleRate, int maxLookaheadSamples)
{
    sampleRate = newSampleRate;

    for (auto& buffer : delayBuffer)
        buffer.assign ((size_t) juce::jmax (1, maxLookaheadSamples) + 1, 0.0f);

    for (auto* s : { &threshold, &invRatio, &knee, &mix })
        s->reset (sampleRate, 0.01);
    makeupGain.reset (sampleRate, 0.02);

    rmsCoef = coefficient (15.0, sampleRate);
    avgUpCoef = coefficient (600.0, sampleRate);
    avgDownCoef = coefficient (2500.0, sampleRate);
    appliedHpf = -1.0f;

    reset();
}

void Compressor::reset()
{
    peak.fill (0.0f);
    meanSquare.fill (0.0f);
    grDb.fill (0.0f);
    avgGr.fill (0.0f);
    for (auto& s : hpfState)
        s.fill (0.0);
    for (auto& buffer : delayBuffer)
        std::fill (buffer.begin(), buffer.end(), 0.0f);

    delayPos = 0;
    snapNext = true;
    grMeter = 0.0f;
    levelMeter = -120.0f;
}

void Compressor::updateHighPass (float hz)
{
    appliedHpf = hz;
    hpfActive = hz > 20.5f;

    if (! hpfActive)
        return;

    const double w0 = 2.0 * pi * juce::jmin ((double) hz, sampleRate * 0.45) / sampleRate;
    const double cosW = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * 0.70710678);
    const double a0 = 1.0 + alpha;

    hpf.b0 = (1.0 + cosW) * 0.5 / a0;
    hpf.b1 = -(1.0 + cosW) / a0;
    hpf.b2 = hpf.b0;
    hpf.a1 = -2.0 * cosW / a0;
    hpf.a2 = (1.0 - alpha) / a0;
}

void Compressor::setSettings (const Settings& s)
{
    settings = s;
    settings.lookaheadSamples = juce::jlimit (0, (int) delayBuffer[0].size() - 1, s.lookaheadSamples);
    latency = settings.lookaheadSamples;

    const float inv = inverseRatio (s.ratio);
    const float makeupDb = s.autoMakeup ? autoMakeupDb (s.thresholdDb, inv) : s.makeupDb;
    const float makeup = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, maxMakeupDb, makeupDb));

    attackCoef = coefficient (s.attackMs, sampleRate);
    releaseCoef = coefficient (s.releaseMs, sampleRate);
    // The peak detector forgets fast for short releases, but slowly enough not to ripple at mid frequencies
    peakDecay = coefficient (s.autoRelease ? 20.0 : juce::jlimit (5.0, 20.0, 0.25 * (double) s.releaseMs), sampleRate);

    if (std::abs (s.scHighPassHz - appliedHpf) > 0.01f)
        updateHighPass (s.scHighPassHz);

    if (snapNext)
    {
        threshold.setCurrentAndTargetValue (s.thresholdDb);
        invRatio.setCurrentAndTargetValue (inv);
        knee.setCurrentAndTargetValue (s.kneeDb);
        mix.setCurrentAndTargetValue (s.mix);
        makeupGain.setCurrentAndTargetValue (makeup);
        snapNext = false;
    }
    else
    {
        threshold.setTargetValue (s.thresholdDb);
        invRatio.setTargetValue (inv);
        knee.setTargetValue (s.kneeDb);
        mix.setTargetValue (s.mix);
        makeupGain.setTargetValue (makeup);
    }
}

void Compressor::process (float* left, float* right, int numSamples)
{
    float* io[numChannels] = { left, right };
    const int bufferSize = (int) delayBuffer[0].size();
    const float linkAmount = juce::jlimit (0.0f, 1.0f, settings.link);

    float maxGr = 0.0f, maxLevel = -120.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float thr = threshold.getNextValue();
        const float inv = invRatio.getNextValue();
        const float kn = knee.getNextValue();
        const float wet = mix.getNextValue();
        const float makeup = makeupGain.getNextValue();

        std::array<float, numChannels> input {}, target {};

        for (int c = 0; c < numChannels; ++c)
        {
            input[(size_t) c] = io[c][i];
            double x = input[(size_t) c];

            if (hpfActive)
            {
                auto& z = hpfState[(size_t) c];
                const double y = hpf.b0 * x + z[0];
                z[0] = hpf.b1 * x - hpf.a1 * y + z[1];
                z[1] = hpf.b2 * x - hpf.a2 * y;
                x = y;
            }

            float levelDb;
            if (settings.rms)
            {
                float& ms = meanSquare[(size_t) c];
                ms += ((float) (x * x) - ms) * (1.0f - rmsCoef);
                if (ms < 1.0e-20f)
                    ms = 0.0f;
                // 2 * mean square makes a full-scale sine read 0 dB, like the peak detector
                levelDb = 10.0f * std::log10 (juce::jmax (2.0f * ms, 1.0e-12f));
            }
            else
            {
                float& env = peak[(size_t) c];
                env = juce::jmax (std::abs ((float) x), env * peakDecay);
                if (env < 1.0e-9f)
                    env = 0.0f;
                levelDb = toDb (env);
            }

            maxLevel = juce::jmax (maxLevel, levelDb);
            target[(size_t) c] = gainReductionDb (levelDb, thr, inv, kn);
        }

        const float strongest = juce::jmax (target[0], target[1]);

        for (int c = 0; c < numChannels; ++c)
        {
            const float wanted = target[(size_t) c] + linkAmount * (strongest - target[(size_t) c]);
            float& gr = grDb[(size_t) c];
            float& avg = avgGr[(size_t) c];

            if (wanted > gr)
                gr = wanted + (gr - wanted) * attackCoef;
            else
            {
                float coef = releaseCoef;
                if (settings.autoRelease)
                {
                    // Short bursts release quickly, sustained heavy compression releases slowly
                    const float depth = juce::jlimit (0.0f, 1.0f, avg / 10.0f);
                    coef = coefficient (40.0 + 1100.0 * (double) (depth * std::sqrt (depth)), sampleRate);
                }
                gr = wanted + (gr - wanted) * coef;
                if (gr < 1.0e-5f && wanted <= 0.0f)
                    gr = 0.0f;
            }

            avg = gr + (avg - gr) * (gr > avg ? avgUpCoef : avgDownCoef);
            if (avg < 1.0e-5f)
                avg = 0.0f;
        }

        maxGr = juce::jmax (maxGr, grDb[0], grDb[1]);

        for (int c = 0; c < numChannels; ++c)
        {
            auto& buffer = delayBuffer[(size_t) c];
            float dry = input[(size_t) c];

            if (latency > 0)
            {
                buffer[(size_t) delayPos] = dry;
                int read = delayPos - latency;
                if (read < 0)
                    read += bufferSize;
                dry = buffer[(size_t) read];
            }

            const float gain = std::exp (-grDb[(size_t) c] * dbToNepers) * makeup;
            io[c][i] = dry + wet * (dry * gain - dry);
        }

        if (latency > 0 && ++delayPos >= bufferSize)
            delayPos = 0;
    }

    grMeter = maxGr;
    levelMeter = maxLevel;
}
} // namespace compwoody
