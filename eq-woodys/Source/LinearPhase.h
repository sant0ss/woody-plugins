#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include "FilterDesign.h"

namespace eqwoody
{
// Length of the FIR filter for each quality setting; the latency is half of it.
int impulseLength (int quality);

// The EQ as a 2x2 matrix of zero-phase filters: out L = LL*L + LR*R, out R = RL*L + RR*R.
// Bands that act on both channels equally only use the diagonal; mid/side bands also need the cross terms.
struct ImpulseSet
{
    std::array<std::vector<float>, 4> ir; // LL, LR, RL, RR
    bool needsCross = false;
};

// Evaluates every band's magnitude response on a dense frequency grid, combines them as matrices
// (so mid/side bands are exact), and turns each result into a symmetric (linear-phase) impulse
// response with an inverse FFT and a window.
ImpulseSet buildImpulses (const BandsArray&, double sampleRate, int length);

// Applies an ImpulseSet by partitioned convolution. Impulse responses can be swapped while audio runs.
class LinearPhaseFilter
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void load (const ImpulseSet&, double sampleRate);

    // In place on a stereo pair.
    void process (float* left, float* right, int numSamples, bool useCross);

private:
    std::array<juce::dsp::Convolution, 4> engines; // LL, LR, RL, RR
    std::array<std::vector<float>, 4> scratch;
};
} // namespace eqwoody
