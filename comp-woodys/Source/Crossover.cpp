#include "Crossover.h"
#include <cmath>

namespace compwoody
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double butterworthQ = 0.70710678118654752;
constexpr int subBlock = 32; // coefficients follow the smoothed frequencies this often
} // namespace

std::array<float, numCrossovers> orderedCrossovers (std::array<float, numCrossovers> f, double sampleRate)
{
    const float top = (float) juce::jmin (20000.0, sampleRate * 0.45);

    f[0] = juce::jmax (f[0], minCrossoverFreq);
    for (int i = 1; i < numCrossovers; ++i)
        f[(size_t) i] = juce::jmax (f[(size_t) i], f[(size_t) i - 1] * minCrossoverSpacing);

    f[numCrossovers - 1] = juce::jmin (f[numCrossovers - 1], top);
    for (int i = numCrossovers - 2; i >= 0; --i)
        f[(size_t) i] = juce::jmin (f[(size_t) i], f[(size_t) i + 1] / minCrossoverSpacing);

    return f;
}

void BandSplitter::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& f : freqs)
        f.reset (sampleRate, 0.03);
    reset();
}

void BandSplitter::reset()
{
    for (auto& channel : channels)
    {
        for (auto& pair : channel.lowPass)
            for (auto& s : pair)
                s.z = {};
        for (auto& pair : channel.highPass)
            for (auto& s : pair)
                s.z = {};
        for (auto& s : channel.allPass)
            s.z = {};
    }
}

void BandSplitter::setCrossovers (const std::array<float, numCrossovers>& wanted, bool immediately)
{
    const auto f = orderedCrossovers (wanted, sampleRate);

    for (size_t i = 0; i < numCrossovers; ++i)
    {
        if (immediately)
            freqs[i].setCurrentAndTargetValue (f[i]);
        else
            freqs[i].setTargetValue (f[i]);
    }

    if (immediately)
    {
        lastDesigned = {};
        updateCoefficients();
    }
}

void BandSplitter::updateCoefficients()
{
    for (size_t i = 0; i < numCrossovers; ++i)
    {
        const float hz = freqs[i].getCurrentValue();
        if (hz == lastDesigned[i])
            continue;

        lastDesigned[i] = hz;

        const double w0 = 2.0 * pi * (double) hz / sampleRate;
        const double cosW = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * butterworthQ);
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cosW / a0;
        const double a2 = (1.0 - alpha) / a0;

        lowCoeffs[i] = { (1.0 - cosW) * 0.5 / a0, (1.0 - cosW) / a0, (1.0 - cosW) * 0.5 / a0, a1, a2 };
        highCoeffs[i] = { (1.0 + cosW) * 0.5 / a0, -(1.0 + cosW) / a0, (1.0 + cosW) * 0.5 / a0, a1, a2 };
        allPassCoeffs[i] = { (1.0 - alpha) / a0, a1, (1.0 + alpha) / a0, a1, a2 };
    }
}

void BandSplitter::split (const float* const* input, float* const* const* bands, int numSamples)
{
    for (int start = 0; start < numSamples; start += subBlock)
    {
        const int count = juce::jmin (subBlock, numSamples - start);

        for (auto& f : freqs)
            f.skip (count);
        updateCoefficients();

        for (size_t c = 0; c < channels.size(); ++c)
        {
            auto& ch = channels[c];
            const float* in = input[c] + start;

            for (int i = 0; i < count; ++i)
            {
                const double x = in[i];

                const double low1 = ch.lowPass[0][1].run (lowCoeffs[0], ch.lowPass[0][0].run (lowCoeffs[0], x));
                const double high1 = ch.highPass[0][1].run (highCoeffs[0], ch.highPass[0][0].run (highCoeffs[0], x));

                const double low2 = ch.lowPass[1][1].run (lowCoeffs[1], ch.lowPass[1][0].run (lowCoeffs[1], high1));
                const double high2 = ch.highPass[1][1].run (highCoeffs[1], ch.highPass[1][0].run (highCoeffs[1], high1));

                const double low3 = ch.lowPass[2][1].run (lowCoeffs[2], ch.lowPass[2][0].run (lowCoeffs[2], high2));
                const double high3 = ch.highPass[2][1].run (highCoeffs[2], ch.highPass[2][0].run (highCoeffs[2], high2));

                const double band0 = ch.allPass[1].run (allPassCoeffs[2], ch.allPass[0].run (allPassCoeffs[1], low1));
                const double band1 = ch.allPass[2].run (allPassCoeffs[2], low2);

                bands[c][0][start + i] = (float) band0;
                bands[c][1][start + i] = (float) band1;
                bands[c][2][start + i] = (float) low3;
                bands[c][3][start + i] = (float) high3;
            }
        }
    }
}
} // namespace compwoody
