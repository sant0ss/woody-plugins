#include "LinearPhase.h"
#include <cmath>
#include <complex>

namespace eqwoody
{
namespace
{
constexpr double pi = 3.14159265358979323846;

// Plain radix-2 FFT in double precision (used off the audio thread, so clarity beats speed).
void fft (std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap (a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = 2.0 * pi / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> step (std::cos (angle), std::sin (angle));

        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= step;
            }
        }
    }

    if (inverse)
        for (auto& v : a)
            v /= (double) n;
}
} // namespace

int impulseLength (int quality)
{
    static const int lengths[] = { 2048, 4096, 8192, 16384 };
    return lengths[juce::jlimit (0, 3, quality)];
}

ImpulseSet buildImpulses (const BandsArray& bands, double sampleRate, int length)
{
    const int half = length / 2;

    // T = product of the band matrices, per frequency bin
    std::vector<double> t00 ((size_t) half + 1, 1.0), t01 ((size_t) half + 1, 0.0),
                        t10 ((size_t) half + 1, 0.0), t11 ((size_t) half + 1, 1.0);

    ImpulseSet result;

    for (const auto& band : bands)
    {
        if (! isActive (band))
            continue;

        const auto sections = designBand (band, sampleRate);
        if (sections.count == 0)
            continue;

        if (band.place == Mid || band.place == Side)
            result.needsCross = true;

        for (int k = 0; k <= half; ++k)
        {
            const double g = magnitude (sections, 2.0 * pi * (double) k / (double) length);

            double m00 = 1.0, m01 = 0.0, m10 = 0.0, m11 = 1.0;
            switch (band.place)
            {
                case Stereo: m00 = m11 = g; break;
                case Left:   m00 = g; break;
                case Right:  m11 = g; break;
                case Mid:    m00 = m11 = (g + 1.0) * 0.5; m01 = m10 = (g - 1.0) * 0.5; break;
                case Side:   m00 = m11 = (g + 1.0) * 0.5; m01 = m10 = -(g - 1.0) * 0.5; break;
                default: break;
            }

            const auto i = (size_t) k;
            const double n00 = m00 * t00[i] + m01 * t10[i];
            const double n01 = m00 * t01[i] + m01 * t11[i];
            const double n10 = m10 * t00[i] + m11 * t10[i];
            const double n11 = m10 * t01[i] + m11 * t11[i];
            t00[i] = n00; t01[i] = n01; t10[i] = n10; t11[i] = n11;
        }
    }

    const std::vector<double>* responses[4] = { &t00, &t01, &t10, &t11 };

    std::vector<std::complex<double>> spectrum ((size_t) length);

    for (size_t e = 0; e < 4; ++e)
    {
        const auto& response = *responses[e];

        // A real, even spectrum: its inverse transform is a real, symmetric (zero-phase) impulse
        for (int k = 0; k <= half; ++k)
            spectrum[(size_t) k] = response[(size_t) k];
        for (int k = 1; k < half; ++k)
            spectrum[(size_t) (length - k)] = response[(size_t) k];

        fft (spectrum, true);

        // Shift the centre to length/2 so the filter is causal, and window the ends
        auto& ir = result.ir[e];
        ir.assign ((size_t) length, 0.0f);

        for (int n = 0; n < length; ++n)
        {
            const double window = 0.42 - 0.5 * std::cos (2.0 * pi * n / length) + 0.08 * std::cos (4.0 * pi * n / length);
            const size_t source = (size_t) ((n - half + length) % length);
            ir[(size_t) n] = (float) (spectrum[source].real() * window);
        }
    }

    return result;
}

//==============================================================================
void LinearPhaseFilter::prepare (double sampleRate, int maxBlockSize)
{
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };

    for (auto& engine : engines)
        engine.prepare (spec);

    for (auto& s : scratch)
        s.assign ((size_t) maxBlockSize, 0.0f);
}

void LinearPhaseFilter::reset()
{
    for (auto& engine : engines)
        engine.reset();
}

void LinearPhaseFilter::load (const ImpulseSet& set, double sampleRate)
{
    for (size_t e = 0; e < 4; ++e)
    {
        juce::AudioBuffer<float> buffer (1, (int) set.ir[e].size());
        std::copy (set.ir[e].begin(), set.ir[e].end(), buffer.getWritePointer (0));

        // No trimming and no normalising: the position of the centre tap is the latency
        engines[e].loadImpulseResponse (std::move (buffer), sampleRate,
                                        juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no,
                                        juce::dsp::Convolution::Normalise::no);
    }
}

void LinearPhaseFilter::process (float* left, float* right, int numSamples, bool useCross)
{
    const float* inputs[4] = { left, right, left, right };

    for (size_t e = 0; e < 4; ++e)
    {
        if ((e == 1 || e == 2) && ! useCross)
            continue;

        const float* inPtr = inputs[e];
        float* outPtr = scratch[e].data();
        juce::dsp::AudioBlock<const float> in (&inPtr, 1, (size_t) numSamples);
        juce::dsp::AudioBlock<float> out (&outPtr, 1, (size_t) numSamples);
        engines[e].process (juce::dsp::ProcessContextNonReplacing<float> (in, out));
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = scratch[0][(size_t) i] + (useCross ? scratch[1][(size_t) i] : 0.0f);
        const float r = scratch[3][(size_t) i] + (useCross ? scratch[2][(size_t) i] : 0.0f);
        left[i] = l;
        right[i] = r;
    }
}
} // namespace eqwoody
