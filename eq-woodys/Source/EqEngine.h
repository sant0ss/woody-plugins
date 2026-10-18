#pragma once

#include <vector>
#include "FilterDesign.h"
#include "WoodySaturation.h"

namespace eqwoody
{
// Zero-latency (minimum phase) EQ: every band is a cascade of biquads, and a band can act on
// L, R, both, or the mid or side signal. Also computes the per-band saturation.
class EqEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // In place on a stereo pair.
    void processMinPhase (float* left, float* right, int numSamples, const BandsArray&);

    // Reads the input and writes only the harmonic content the saturating bands add (zeroed first).
    // Each band saturates just the part of the signal around its own frequency.
    void processSaturation (const float* left, const float* right, int numSamples, const BandsArray&,
                            float* satLeft, float* satRight);

private:
    struct BandRuntime
    {
        SectionSet start, target;
        double z1[maxSections][2] {};
        double z2[maxSections][2] {};
        bool wasActive = false;
    };

    struct SatRuntime
    {
        double z1[2] {}, z2[2] {};
        std::array<woody::Saturator, 2> shaper;
        std::array<woody::DcBlocker, 2> dc;
        bool wasActive = false;
    };

    void runBand (BandRuntime&, float* signal, int channel, int numSamples);

    double sampleRate = 44100.0;
    std::array<BandRuntime, numBands> bands;
    std::array<SatRuntime, numBands> sats;
    std::vector<float> mid, side;
};
} // namespace eqwoody
