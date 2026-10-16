#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace woody::dsp
{
// Stereo biquad (transposed direct form II) with RBJ cookbook designs.
class Biquad
{
public:
    void reset()
    {
        z1[0] = z1[1] = z2[0] = z2[1] = 0.0f;
    }

    void setLowShelf (double sampleRate, double freq, double gainDb)   { design (sampleRate, freq, 0.707, gainDb, Type::lowShelf); }
    void setHighShelf (double sampleRate, double freq, double gainDb)  { design (sampleRate, freq, 0.707, gainDb, Type::highShelf); }
    void setPeak (double sampleRate, double freq, double q, double gainDb) { design (sampleRate, freq, q, gainDb, Type::peak); }
    void setHighPass (double sampleRate, double freq)                  { design (sampleRate, freq, 0.707, 0.0, Type::highPass); }

    bool isBypassed() const { return bypassed; }

    void process (float* left, float* right, int numSamples)
    {
        if (bypassed)
            return;

        float* channels[2] = { left, right };
        for (int ch = 0; ch < 2; ++ch)
        {
            float* x = channels[ch];
            float s1 = z1[ch], s2 = z2[ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const float in = x[i];
                const float out = b0 * in + s1;
                s1 = b1 * in - a1 * out + s2;
                s2 = b2 * in - a2 * out;
                x[i] = out;
            }

            z1[ch] = s1;
            z2[ch] = s2;
        }
    }

private:
    enum class Type { lowShelf, highShelf, peak, highPass };

    void design (double sampleRate, double freq, double q, double gainDb, Type type)
    {
        // A flat shelf/peak costs nothing
        bypassed = type != Type::highPass && std::abs (gainDb) < 0.05;
        if (bypassed)
            return;

        const double a = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sampleRate * 0.45, freq) / sampleRate;
        const double cosw = std::cos (w0), sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * q);
        double nb0, nb1, nb2, na0, na1, na2;

        switch (type)
        {
            case Type::lowShelf:
            {
                const double sq = 2.0 * std::sqrt (a) * alpha;
                nb0 = a * ((a + 1) - (a - 1) * cosw + sq);
                nb1 = 2 * a * ((a - 1) - (a + 1) * cosw);
                nb2 = a * ((a + 1) - (a - 1) * cosw - sq);
                na0 = (a + 1) + (a - 1) * cosw + sq;
                na1 = -2 * ((a - 1) + (a + 1) * cosw);
                na2 = (a + 1) + (a - 1) * cosw - sq;
                break;
            }
            case Type::highShelf:
            {
                const double sq = 2.0 * std::sqrt (a) * alpha;
                nb0 = a * ((a + 1) + (a - 1) * cosw + sq);
                nb1 = -2 * a * ((a - 1) + (a + 1) * cosw);
                nb2 = a * ((a + 1) + (a - 1) * cosw - sq);
                na0 = (a + 1) - (a - 1) * cosw + sq;
                na1 = 2 * ((a - 1) - (a + 1) * cosw);
                na2 = (a + 1) - (a - 1) * cosw - sq;
                break;
            }
            case Type::peak:
                nb0 = 1 + alpha * a;
                nb1 = -2 * cosw;
                nb2 = 1 - alpha * a;
                na0 = 1 + alpha / a;
                na1 = -2 * cosw;
                na2 = 1 - alpha / a;
                break;
            case Type::highPass:
            default:
                nb0 = (1 + cosw) / 2;
                nb1 = -(1 + cosw);
                nb2 = (1 + cosw) / 2;
                na0 = 1 + alpha;
                na1 = -2 * cosw;
                na2 = 1 - alpha;
                break;
        }

        b0 = (float) (nb0 / na0);
        b1 = (float) (nb1 / na0);
        b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0);
        a2 = (float) (na2 / na0);
    }

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    bool bypassed = true;
};

// Stereo-linked feed-forward compressor with a peak envelope.
class Compressor
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        envelope = 0.0f;
    }

    void reset() { envelope = 0.0f; }

    void setParameters (float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb)
    {
        threshold = thresholdDb;
        slope = 1.0f - 1.0f / juce::jmax (1.0f, ratio);
        attackCoeff = std::exp (-1.0f / (0.001f * attackMs * (float) sampleRate));
        releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * (float) sampleRate));
        makeup = juce::Decibels::decibelsToGain (makeupDb);
    }

    void process (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float level = juce::jmax (std::abs (left[i]), std::abs (right[i]));
            const float coeff = level > envelope ? attackCoeff : releaseCoeff;
            envelope = level + coeff * (envelope - level);

            const float levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
            const float overDb = levelDb - threshold;
            const float gain = overDb > 0.0f ? juce::Decibels::decibelsToGain (-overDb * slope) * makeup : makeup;

            left[i] *= gain;
            right[i] *= gain;
        }
    }

private:
    double sampleRate = 44100.0;
    float threshold = 0.0f, slope = 0.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f, makeup = 1.0f;
    float envelope = 0.0f;
};

// Boosts or cuts the attack and the sustain of a hit by comparing a fast and a slow envelope.
class TransientShaper
{
public:
    void prepare (double sampleRate)
    {
        fastAttack  = std::exp (-1.0f / (0.0005f * (float) sampleRate));
        fastRelease = std::exp (-1.0f / (0.030f * (float) sampleRate));
        slowAttack  = std::exp (-1.0f / (0.015f * (float) sampleRate));
        slowRelease = std::exp (-1.0f / (0.150f * (float) sampleRate));
        reset();
    }

    void reset()
    {
        fast = slow = 0.0f;
        gain = 1.0f;
    }

    // attack and sustain in -1..1
    void process (float* left, float* right, int numSamples, float attack, float sustain)
    {
        if (std::abs (attack) < 0.01f && std::abs (sustain) < 0.01f)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float level = juce::jmax (std::abs (left[i]), std::abs (right[i])) + 1.0e-6f;
            fast = level + (level > fast ? fastAttack : fastRelease) * (fast - level);
            slow = level + (level > slow ? slowAttack : slowRelease) * (slow - level);

            // > 0 while the hit is starting, < 0 while it rings out
            const float diff = juce::jlimit (-1.0f, 1.0f, std::log2 ((fast + 1.0e-6f) / (slow + 1.0e-6f)));
            const float db = diff > 0.0f ? attack * 15.0f * diff : sustain * 12.0f * -diff;
            const float target = juce::Decibels::decibelsToGain (db);
            gain += 0.05f * (target - gain);

            left[i] *= gain;
            right[i] *= gain;
        }
    }

private:
    float fastAttack = 0, fastRelease = 0, slowAttack = 0, slowRelease = 0;
    float fast = 0, slow = 0, gain = 1;
};

// Soft clipper with unity gain for quiet signals: drive only rounds off the peaks.
inline void saturate (float* left, float* right, int numSamples, float drive)
{
    if (drive < 0.01f)
        return;

    const float g = 1.0f + drive * 6.0f;
    const float makeup = 1.0f + drive * 0.6f;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i]  = std::tanh (left[i] * g) / g * makeup;
        right[i] = std::tanh (right[i] * g) / g * makeup;
    }
}

// Keeps the output below 0 dBFS with a smooth knee.
inline float softLimit (float x)
{
    const float a = std::abs (x);
    if (a <= 0.9f)
        return x;
    const float limited = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
    return x < 0.0f ? -limited : limited;
}
} // namespace woody::dsp
