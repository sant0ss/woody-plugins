#include "EqEngine.h"
#include <cmath>

namespace eqwoody
{
namespace
{
// One biquad (transposed direct form II) with the coefficients sliding linearly from a to b over the block,
// so moving a band never produces a zipper noise or a click.
void runSection (const Biquad& a, const Biquad& b, float* x, int n, double& z1, double& z2)
{
    const double inv = 1.0 / (double) n;
    double c0 = a.b0, c1 = a.b1, c2 = a.b2, d1 = a.a1, d2 = a.a2;
    const double s0 = (b.b0 - a.b0) * inv, s1 = (b.b1 - a.b1) * inv, s2 = (b.b2 - a.b2) * inv;
    const double t1 = (b.a1 - a.a1) * inv, t2 = (b.a2 - a.a2) * inv;

    for (int i = 0; i < n; ++i)
    {
        const double in = x[i];
        const double out = c0 * in + z1;
        z1 = c1 * in - d1 * out + z2;
        z2 = c2 * in - d2 * out;
        x[i] = (float) out;

        c0 += s0; c1 += s1; c2 += s2; d1 += t1; d2 += t2;
    }
}
} // namespace

void EqEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    mid.assign ((size_t) maxBlockSize, 0.0f);
    side.assign ((size_t) maxBlockSize, 0.0f);
    reset();
}

void EqEngine::reset()
{
    for (auto& b : bands)
        b = {};

    for (auto& s : sats)
        s = {};
}

void EqEngine::runBand (BandRuntime& band, float* signal, int channel, int numSamples)
{
    for (int s = 0; s < band.target.count; ++s)
    {
        const auto& to = band.target.sections[(size_t) s];
        const auto& from = s < band.start.count ? band.start.sections[(size_t) s] : to;
        runSection (from, to, signal, numSamples, band.z1[s][channel], band.z2[s][channel]);
    }
}

void EqEngine::processMinPhase (float* left, float* right, int numSamples, const BandsArray& settings)
{
    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        auto& band = bands[i];
        const auto& s = settings[i];

        if (! isActive (s))
        {
            band = {};
            continue;
        }

        band.target = designBand (s, sampleRate);
        if (! band.wasActive)
        {
            band.start = band.target;
            band.wasActive = true;
        }

        switch (s.place)
        {
            case Stereo:
                runBand (band, left, 0, numSamples);
                runBand (band, right, 1, numSamples);
                break;

            case Left:
                runBand (band, left, 0, numSamples);
                break;

            case Right:
                runBand (band, right, 1, numSamples);
                break;

            case Mid:
            case Side:
            {
                // Convert to mid/side, filter only one of them, and convert back
                for (int n = 0; n < numSamples; ++n)
                {
                    mid[(size_t) n] = 0.5f * (left[n] + right[n]);
                    side[(size_t) n] = 0.5f * (left[n] - right[n]);
                }

                if (s.place == Mid)
                    runBand (band, mid.data(), 0, numSamples);
                else
                    runBand (band, side.data(), 1, numSamples);

                for (int n = 0; n < numSamples; ++n)
                {
                    left[n] = mid[(size_t) n] + side[(size_t) n];
                    right[n] = mid[(size_t) n] - side[(size_t) n];
                }
                break;
            }

            default:
                break;
        }

        band.start = band.target;
    }
}

void EqEngine::processSaturation (const float* left, const float* right, int numSamples, const BandsArray& settings,
                                  float* satLeft, float* satRight)
{
    std::fill (satLeft, satLeft + numSamples, 0.0f);
    std::fill (satRight, satRight + numSamples, 0.0f);

    for (size_t i = 0; i < (size_t) numBands; ++i)
    {
        auto& rt = sats[i];
        const auto& s = settings[i];

        if (! s.on || s.sat < 0.001f)
        {
            rt = {};
            continue;
        }

        rt.wasActive = true;

        const Biquad bp = designBandPass (s.freq, juce::jmax (0.3f, s.q), sampleRate);
        const auto mode = (woody::SatMode) juce::jlimit (0, 4, s.satMode);
        const float drive = 1.0f + s.sat * 14.0f;
        const float amount = s.sat;

        // Isolates the band, saturates it, and returns only the new content (the curve minus the input)
        auto harmonics = [&] (float x, int channel)
        {
            const double out = bp.b0 * x + rt.z1[channel];
            rt.z1[channel] = bp.b1 * x - bp.a1 * out + rt.z2[channel];
            rt.z2[channel] = bp.b2 * x - bp.a2 * out;

            const float band = (float) out;
            const float shaped = rt.shaper[(size_t) channel].process (mode, band * drive) / drive;
            return rt.dc[(size_t) channel].process ((shaped - band) * amount);
        };

        for (int n = 0; n < numSamples; ++n)
        {
            switch (s.place)
            {
                case Stereo:
                    satLeft[n] += harmonics (left[n], 0);
                    satRight[n] += harmonics (right[n], 1);
                    break;

                case Left:
                    satLeft[n] += harmonics (left[n], 0);
                    break;

                case Right:
                    satRight[n] += harmonics (right[n], 1);
                    break;

                case Mid:
                {
                    const float d = harmonics (0.5f * (left[n] + right[n]), 0);
                    satLeft[n] += d;
                    satRight[n] += d;
                    break;
                }

                case Side:
                {
                    const float d = harmonics (0.5f * (left[n] - right[n]), 1);
                    satLeft[n] += d;
                    satRight[n] -= d;
                    break;
                }

                default:
                    break;
            }
        }
    }
}
} // namespace eqwoody
