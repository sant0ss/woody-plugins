#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace compwoody
{
constexpr int numBands = 4;
constexpr int numCrossovers = numBands - 1;
constexpr float minCrossoverSpacing = 1.5f; // frequency ratio between neighbouring crossovers
constexpr float minCrossoverFreq = 25.0f;

// Keeps the crossover frequencies ordered, at least minCrossoverSpacing apart and below Nyquist
std::array<float, numCrossovers> orderedCrossovers (std::array<float, numCrossovers> freqs, double sampleRate);

// Splits two channels into four bands with 4th-order Linkwitz-Riley crossovers.
// Every band that passes a later crossover goes through the all-pass of that crossover, so the bands
// sum back to a flat magnitude response (the sum is the input through a 6th-order all-pass).
class BandSplitter
{
public:
    void prepare (double sampleRate);
    void reset();
    void setCrossovers (const std::array<float, numCrossovers>& freqs, bool immediately);

    // Writes numBands signals per channel; each output must hold numSamples values
    void split (const float* const* input, float* const* const* bands, int numSamples);

private:
    struct Coeffs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    struct Stage
    {
        std::array<double, 2> z {};

        double run (const Coeffs& c, double x)
        {
            const double y = c.b0 * x + z[0];
            z[0] = c.b1 * x - c.a1 * y + z[1];
            z[1] = c.b2 * x - c.a2 * y;
            return y;
        }
    };

    struct Channel
    {
        // Two cascaded Butterworth sections make one Linkwitz-Riley filter
        std::array<std::array<Stage, 2>, numCrossovers> lowPass, highPass;
        std::array<Stage, 3> allPass; // crossover 2 and 3 on band 1, crossover 3 on band 2
    };

    void updateCoefficients();

    double sampleRate = 48000.0;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numCrossovers> freqs;
    std::array<float, numCrossovers> lastDesigned {};
    std::array<Coeffs, numCrossovers> lowCoeffs, highCoeffs, allPassCoeffs;
    std::array<Channel, 2> channels;
};
} // namespace compwoody
