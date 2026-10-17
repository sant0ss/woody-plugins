#pragma once

#include <vector>

namespace synthwoody
{
constexpr int tableSize = 2048;
constexpr int numMips = 10;     // each mip halves the number of harmonics, so higher notes alias less
constexpr int numTables = 6;
constexpr int numFrames = 8;    // key frames per table; the position control morphs between them

// Band-limited wavetables, generated once from harmonic spectra.
class WavetableBank
{
public:
    static const WavetableBank& get();

    // Mip level that keeps every harmonic below Nyquist for this frequency.
    static int mipFor (double frequency, double sampleRate);

    // position 0..1 morphs through the frames, phase 0..1 reads one cycle.
    float read (int table, float position, int mip, float phase) const
    {
        const float framePos = position * (float) (numFrames - 1);
        const int f0 = (int) framePos;
        const int f1 = f0 + 1 < numFrames ? f0 + 1 : f0;
        const float blend = framePos - (float) f0;

        const float index = phase * (float) tableSize;
        const int i = (int) index;
        const float frac = index - (float) i;

        const float* a = frame (table, f0, mip);
        const float x = a[i] + frac * (a[i + 1] - a[i]);
        if (blend <= 0.0f || f1 == f0)
            return x;

        const float* b = frame (table, f1, mip);
        const float y = b[i] + frac * (b[i + 1] - b[i]);
        return x + blend * (y - x);
    }

private:
    WavetableBank();

    // tableSize + 1 samples (one guard point for interpolation)
    const float* frame (int table, int frameIndex, int mip) const
    {
        return data.data() + (((size_t) table * numFrames + (size_t) frameIndex) * numMips + (size_t) mip) * (tableSize + 1);
    }

    std::vector<float> data;
};

extern const char* const wavetableNames[numTables];
} // namespace synthwoody
