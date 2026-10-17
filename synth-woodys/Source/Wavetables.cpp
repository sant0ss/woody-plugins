#include "Wavetables.h"
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

namespace synthwoody
{
const char* const wavetableNames[numTables] = { "Basic", "Digital", "Vocal", "Organ", "Glass", "Growl" };

namespace
{
// Harmonics kept at each mip level, most to fewest
constexpr int harmonicsPerMip[numMips] = { 1000, 500, 250, 125, 62, 31, 15, 7, 3, 1 };

constexpr double pi = 3.14159265358979323846;

float pseudoRandom (float x)
{
    const float s = std::sin (x * 12.9898f) * 43758.5453f;
    return s - std::floor (s);
}

// Amplitude of harmonic h (1-based) for one key frame of one table.
float harmonicAmplitude (int table, int frame, int h)
{
    const float fh = (float) h;
    const float inv = 1.0f / fh;

    switch (table)
    {
        case 0: // Basic: sine -> triangle -> saw -> square -> pulses -> brighter saws
        {
            switch (frame)
            {
                case 0:  return h == 1 ? 1.0f : 0.0f;
                case 1:  return (h % 2 == 1) ? ((h % 4 == 1) ? 1.0f : -1.0f) * inv * inv : 0.0f;
                case 2:  return inv;
                case 3:  return (h % 2 == 1) ? inv : 0.0f;
                case 4:  return inv * std::abs (std::sin ((float) pi * fh * 0.25f));
                case 5:  return inv * std::abs (std::sin ((float) pi * fh * 0.125f));
                case 6:  return inv + (h % 2 == 0 ? 0.7f * inv : 0.0f);
                default: return 1.0f / std::sqrt (fh);
            }
        }

        case 1: // Digital: spectral combs
        {
            if (frame < 4)
                return inv * (0.55f + 0.45f * std::cos (fh * (0.4f + 0.35f * (float) frame)));

            const int step = frame - 2; // 2, 3, 4, 5
            return (h == 1 ? 1.0f : 0.0f) + ((h % step == 0) ? 1.0f / std::sqrt (fh) : 0.0f);
        }

        case 2: // Vocal: vowel formants (a e i o u and in-betweens)
        {
            static const float formants[8][2] = {
                { 800, 1150 }, { 400, 1700 }, { 270, 2140 }, { 450, 800 },
                { 325, 700 },  { 660, 1720 }, { 400, 1300 }, { 300, 1000 } };

            const float f1 = formants[frame][0] / 110.0f;
            const float f2 = formants[frame][1] / 110.0f;
            const float d1 = fh - f1, d2 = fh - f2;
            const float peak = std::exp (-d1 * d1 / (2.0f * 1.6f * 1.6f)) + 0.6f * std::exp (-d2 * d2 / (2.0f * 2.6f * 2.6f));
            return std::sqrt (inv) * peak + 0.02f * inv;
        }

        case 3: // Organ: drawbars adding up
        {
            static const float weights[8][8] = {
                { 1, 0, 0, 0, 0, 0, 0, 0 },          { 1, .8f, 0, 0, 0, 0, 0, 0 },
                { 1, .8f, .6f, 0, 0, 0, 0, 0 },      { 1, .8f, .6f, .5f, 0, 0, 0, 0 },
                { 1, 0, .7f, 0, .5f, 0, 0, 0 },      { 1, .6f, 0, .5f, 0, .4f, 0, .3f },
                { 1, .9f, .8f, .7f, .6f, .5f, .4f, .3f }, { .6f, .9f, 1, .9f, .8f, .7f, .6f, .5f } };
            return h <= 8 ? weights[frame][h - 1] : 0.0f;
        }

        case 4: // Glass: irregular, odd-leaning spectra
        {
            const float r = pseudoRandom (fh * 1.7f + (float) frame * 31.3f);
            return std::pow (inv, 0.7f) * (0.25f + 0.75f * r) * ((h % 2 == 1) ? 1.0f : 0.5f);
        }

        default: // Growl: a saw with a sweeping comb
            return inv * (0.5f + 0.5f * std::cos (fh * (0.15f + 0.22f * (float) frame))) + 0.4f * inv;
    }
}
} // namespace

int WavetableBank::mipFor (double frequency, double sampleRate)
{
    const double allowed = 0.5 * sampleRate / juce::jmax (1.0, frequency);
    for (int m = 0; m < numMips; ++m)
        if ((double) harmonicsPerMip[m] <= allowed)
            return m;

    return numMips - 1;
}

const WavetableBank& WavetableBank::get()
{
    static const WavetableBank bank;
    return bank;
}

WavetableBank::WavetableBank()
{
    data.assign ((size_t) numTables * numFrames * numMips * (tableSize + 1), 0.0f);

    std::vector<double> accumulator ((size_t) tableSize);

    for (int table = 0; table < numTables; ++table)
    {
        for (int frame = 0; frame < numFrames; ++frame)
        {
            std::fill (accumulator.begin(), accumulator.end(), 0.0);

            // Add harmonics from the lowest up; whenever a mip's limit is reached, snapshot it
            for (int h = 1; h <= harmonicsPerMip[0]; ++h)
            {
                const double amplitude = harmonicAmplitude (table, frame, h);

                if (amplitude != 0.0)
                {
                    // sin(h * w * n) by recurrence, one multiply-add per sample
                    const double w = 2.0 * pi * h / tableSize;
                    const double c = 2.0 * std::cos (w);
                    double previous = 0.0, current = std::sin (w);

                    accumulator[0] += 0.0;
                    for (int n = 1; n < tableSize; ++n)
                    {
                        accumulator[(size_t) n] += amplitude * current;
                        const double next = c * current - previous;
                        previous = current;
                        current = next;
                    }
                }

                for (int m = numMips - 1; m >= 0; --m)
                {
                    if (h != harmonicsPerMip[m])
                        continue;

                    double peak = 1.0e-9;
                    for (double v : accumulator)
                        peak = std::max (peak, std::abs (v));

                    const double gain = 0.95 / peak;
                    float* out = data.data() + (((size_t) table * numFrames + (size_t) frame) * numMips + (size_t) m) * (tableSize + 1);

                    for (int n = 0; n < tableSize; ++n)
                        out[n] = (float) (accumulator[(size_t) n] * gain);
                    out[tableSize] = out[0];
                }
            }
        }
    }
}
} // namespace synthwoody
