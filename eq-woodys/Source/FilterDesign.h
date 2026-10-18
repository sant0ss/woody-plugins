#pragma once

#include <array>
#include "EqParams.h"

namespace eqwoody
{
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; // a0 normalised to 1
};

constexpr int maxSections = 8; // 96 dB/oct is a 16th-order filter = 8 biquads

struct SectionSet
{
    std::array<Biquad, maxSections> sections;
    int count = 0;
};

// The filter(s) that realise one band at this sample rate. Empty (count = 0) when the band does nothing.
SectionSet designBand (const BandSettings&, double sampleRate);

// Constant 0 dB peak band pass; used to pick out the part of the signal a band saturates.
Biquad designBandPass (double freq, double q, double sampleRate);

// |H(e^jw)| for w in radians per sample
double magnitude (const Biquad&, double w);
double magnitude (const SectionSet&, double w);
} // namespace eqwoody
