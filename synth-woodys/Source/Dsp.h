#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace synthwoody::dsp
{
// Smooth soft clipper (rational tanh approximation)
inline float fastTanh (float x)
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Removes the aliasing of a discontinuity in a naive oscillator.
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

//==============================================================================
struct EnvCoefs
{
    float attack = 0.0f, decay = 0.0f, release = 0.0f, sustain = 1.0f;
};

inline EnvCoefs makeEnvCoefs (float attackSec, float decaySec, float sustain, float releaseSec, double sampleRate)
{
    auto coefficient = [sampleRate] (float timeSec, float timeConstants)
    {
        return std::exp (-timeConstants / (juce::jmax (0.0005f, timeSec) * (float) sampleRate));
    };

    EnvCoefs c;
    c.attack = coefficient (attackSec, 1.79f);  // aims 20% above full level, so it reaches 1 in attackSec
    c.decay = coefficient (decaySec, 4.6f);     // 99% of the way to the sustain level in decaySec
    c.release = coefficient (releaseSec, 6.9f); // -60 dB in releaseSec
    c.sustain = sustain;
    return c;
}

// Exponential ADSR, the way analog envelopes behave.
struct Envelope
{
    enum class Stage { idle, attack, decay, sustain, release };

    Stage stage = Stage::idle;
    float level = 0.0f;

    void trigger() { stage = Stage::attack; }

    void release()
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }

    float tick (const EnvCoefs& c)
    {
        switch (stage)
        {
            case Stage::attack:
                level = 1.2f + (level - 1.2f) * c.attack;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                level = c.sustain + (level - c.sustain) * c.decay;
                if (level - c.sustain < 1.0e-4f)
                {
                    level = c.sustain;
                    stage = Stage::sustain;
                }
                break;

            case Stage::sustain:
                level = c.sustain;
                break;

            case Stage::release:
                level *= c.release;
                if (level < 1.0e-4f)
                {
                    level = 0.0f;
                    stage = Stage::idle;
                }
                break;

            case Stage::idle:
                break;
        }
        return level;
    }
};

//==============================================================================
struct LadderCoefs
{
    float g = 0.0f, k = 0.0f, drive = 1.0f, makeup = 1.0f;
};

inline LadderCoefs makeLadderCoefs (float cutoffHz, float resonance01, float drive01, float sampleRate)
{
    const float fc = juce::jlimit (20.0f, 0.45f * sampleRate, cutoffHz);
    const float wc = std::tan (juce::MathConstants<float>::pi * fc / sampleRate);

    LadderCoefs c;
    c.g = wc / (1.0f + wc);
    c.k = juce::jlimit (0.0f, 1.0f, resonance01) * 3.9f;
    c.drive = 1.0f + drive01 * 4.0f;
    c.makeup = (1.0f + c.k * 0.3f) / (1.0f + drive01 * 1.2f);
    return c;
}

// Four cascaded one-pole stages with feedback and a soft clipper in the loop, in the style of a transistor ladder.
// Taps give low pass 24/12 dB, band pass and high pass.
struct Ladder
{
    float s[4] {};
    float feedback = 0.0f;

    void reset()
    {
        s[0] = s[1] = s[2] = s[3] = 0.0f;
        feedback = 0.0f;
    }

    float process (float in, const LadderCoefs& c, int type)
    {
        const float u = fastTanh (in * c.drive - c.k * feedback);

        float v = (u - s[0]) * c.g;
        const float y1 = v + s[0];
        s[0] = y1 + v;

        v = (y1 - s[1]) * c.g;
        const float y2 = v + s[1];
        s[1] = y2 + v;

        v = (y2 - s[2]) * c.g;
        const float y3 = v + s[2];
        s[2] = y3 + v;

        v = (y3 - s[3]) * c.g;
        const float y4 = v + s[3];
        s[3] = y4 + v;

        feedback = y4;

        float out;
        switch (type)
        {
            case 1:  out = y2; break;
            case 2:  out = (y2 - y4) * 2.0f; break;
            case 3:  out = u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4; break;
            default: out = y4; break;
        }
        return out * c.makeup;
    }
};
} // namespace synthwoody::dsp
