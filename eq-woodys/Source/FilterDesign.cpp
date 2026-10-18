#include "FilterDesign.h"
#include <cmath>
#include <complex>
#include <juce_core/juce_core.h>

namespace eqwoody
{
namespace
{
constexpr double pi = 3.14159265358979323846;

Biquad make (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

struct Trig
{
    double cosw, sinw, alpha;
};

Trig trig (double freq, double q, double sampleRate)
{
    const double w0 = 2.0 * pi * juce::jlimit (5.0, sampleRate * 0.49, freq) / sampleRate;
    return { std::cos (w0), std::sin (w0), std::sin (w0) / (2.0 * juce::jmax (0.05, q)) };
}

// RBJ audio EQ cookbook
Biquad peak (double f, double q, double gainDb, double sr)
{
    const auto t = trig (f, q, sr);
    const double A = std::pow (10.0, gainDb / 40.0);
    return make (1.0 + t.alpha * A, -2.0 * t.cosw, 1.0 - t.alpha * A,
                 1.0 + t.alpha / A, -2.0 * t.cosw, 1.0 - t.alpha / A);
}

Biquad lowShelf (double f, double q, double gainDb, double sr)
{
    const auto t = trig (f, q, sr);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double s = 2.0 * std::sqrt (A) * t.alpha;
    return make (A * ((A + 1.0) - (A - 1.0) * t.cosw + s),
                 2.0 * A * ((A - 1.0) - (A + 1.0) * t.cosw),
                 A * ((A + 1.0) - (A - 1.0) * t.cosw - s),
                 (A + 1.0) + (A - 1.0) * t.cosw + s,
                 -2.0 * ((A - 1.0) + (A + 1.0) * t.cosw),
                 (A + 1.0) + (A - 1.0) * t.cosw - s);
}

Biquad highShelf (double f, double q, double gainDb, double sr)
{
    const auto t = trig (f, q, sr);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double s = 2.0 * std::sqrt (A) * t.alpha;
    return make (A * ((A + 1.0) + (A - 1.0) * t.cosw + s),
                 -2.0 * A * ((A - 1.0) + (A + 1.0) * t.cosw),
                 A * ((A + 1.0) + (A - 1.0) * t.cosw - s),
                 (A + 1.0) - (A - 1.0) * t.cosw + s,
                 2.0 * ((A - 1.0) - (A + 1.0) * t.cosw),
                 (A + 1.0) - (A - 1.0) * t.cosw - s);
}

Biquad highPass2 (double f, double q, double sr)
{
    const auto t = trig (f, q, sr);
    return make ((1.0 + t.cosw) / 2.0, -(1.0 + t.cosw), (1.0 + t.cosw) / 2.0,
                 1.0 + t.alpha, -2.0 * t.cosw, 1.0 - t.alpha);
}

Biquad lowPass2 (double f, double q, double sr)
{
    const auto t = trig (f, q, sr);
    return make ((1.0 - t.cosw) / 2.0, 1.0 - t.cosw, (1.0 - t.cosw) / 2.0,
                 1.0 + t.alpha, -2.0 * t.cosw, 1.0 - t.alpha);
}

Biquad notch (double f, double q, double sr)
{
    const auto t = trig (f, q, sr);
    return make (1.0, -2.0 * t.cosw, 1.0, 1.0 + t.alpha, -2.0 * t.cosw, 1.0 - t.alpha);
}

// First-order sections give the odd 6 dB/oct steps of the cut slopes
Biquad firstOrder (double f, bool high, double sr)
{
    const double k = std::tan (pi * juce::jlimit (5.0, sr * 0.49, f) / sr);
    const double a1 = (k - 1.0) / (k + 1.0);

    if (high)
        return { 1.0 / (1.0 + k), -1.0 / (1.0 + k), 0.0, a1, 0.0 };

    return { k / (1.0 + k), k / (1.0 + k), 0.0, a1, 0.0 };
}

void add (SectionSet& set, const Biquad& b)
{
    if (set.count < maxSections)
        set.sections[(size_t) set.count++] = b;
}

// Butterworth cut of the given order: an optional first-order section plus second-order sections
// whose Q follows the Butterworth pole positions. A Q above 1 adds resonance at the corner.
void addCut (SectionSet& set, bool high, int order, double f, double userQ, double sr)
{
    if (order % 2 == 1)
        add (set, firstOrder (f, high, sr));

    const int pairs = order / 2;
    for (int k = 1; k <= pairs; ++k)
    {
        double q = 1.0 / (2.0 * std::sin (pi * (2.0 * k - 1.0) / (2.0 * order)));
        if (k == pairs)
            q *= juce::jmax (1.0, userQ);

        add (set, high ? highPass2 (f, q, sr) : lowPass2 (f, q, sr));
    }
}
} // namespace

SectionSet designBand (const BandSettings& b, double sr)
{
    SectionSet set;
    const double f = b.freq;
    const double gain = b.gain;
    const double q = b.q;
    static const int orders[] = { 1, 2, 3, 4, 6, 8, 12, 16 }; // 6 dB/oct per order

    switch (b.type)
    {
        case Bell:
            if (std::abs (gain) >= 0.01)
                add (set, peak (f, q, gain, sr));
            break;

        case LowShelf:
            if (std::abs (gain) >= 0.01)
                add (set, lowShelf (f, juce::jlimit (0.1, 3.0, q), gain, sr));
            break;

        case HighShelf:
            if (std::abs (gain) >= 0.01)
                add (set, highShelf (f, juce::jlimit (0.1, 3.0, q), gain, sr));
            break;

        case LowCut:
            addCut (set, true, orders[juce::jlimit (0, 7, b.slope)], f, q, sr);
            break;

        case HighCut:
            addCut (set, false, orders[juce::jlimit (0, 7, b.slope)], f, q, sr);
            break;

        case Notch:
            add (set, notch (f, q, sr));
            break;

        case BandPass:
            add (set, designBandPass (f, q, sr));
            break;

        case Tilt:
            // A shelf up on one side and down on the other around the same pivot
            if (std::abs (gain) >= 0.01)
            {
                add (set, lowShelf (f, 0.6, gain * 0.5, sr));
                add (set, highShelf (f, 0.6, -gain * 0.5, sr));
            }
            break;
    }

    return set;
}

Biquad designBandPass (double f, double q, double sr)
{
    const auto t = trig (f, q, sr);
    return make (t.alpha, 0.0, -t.alpha, 1.0 + t.alpha, -2.0 * t.cosw, 1.0 - t.alpha);
}

double magnitude (const Biquad& c, double w)
{
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return std::abs (num) / std::max (1.0e-12, std::abs (den));
}

double magnitude (const SectionSet& set, double w)
{
    double m = 1.0;
    for (int i = 0; i < set.count; ++i)
        m *= magnitude (set.sections[(size_t) i], w);
    return m;
}
} // namespace eqwoody
