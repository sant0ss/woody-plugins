#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "WoodySaturation.h"

namespace satwoody
{
struct Settings
{
    float inputDb = 0.0f;
    float drive = 0.3f;          // 0..1, mapped to 0..maxDriveDb of gain into the curve
    int type = 0;                // woody::SatMode
    float tone = 0.0f;           // -1..1 tilt of the saturated signal
    float bias = 0.0f;           // -1..1 offset before the curve (even harmonics)
    float mix = 1.0f;
    float outputDb = 0.0f;
    bool autoGain = false;
    int stereo = 0;              // StereoMode
    float focusLow = 20.0f;      // only what lies between these two is saturated
    float focusHigh = 20000.0f;
    int oversampling = 0;        // 0 = off, 1..3 = 2x, 4x, 8x
    bool bypass = false;
};

// Saturation of a stereo signal: input trim, optional mid/side, focus split, drive, curve (with
// anti-aliasing and optional oversampling), DC removal, tone, auto-gain, dry/wet mix and output trim.
// Everything is allocated in prepare(); process() accepts any block size.
class SatEngine
{
public:
    static constexpr int scopeSize = 4096;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Samples of delay the whole plugin adds for an oversampling choice (0 = off, 1..3 = 2x..8x)
    int latencyFor (int oversamplingIndex) const;

    void process (float* left, float* right, int numSamples, const Settings&);

    // Readable from any thread
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    std::atomic<float> curveInput { 0.0f };          // the value that last entered the curve
    std::array<std::atomic<float>, scopeSize> scopeDry, scopeWet;
    std::atomic<int> scopePosition { 0 };

private:
    struct Delay
    {
        std::vector<float> buffer;
        int position = 0;

        void init (int size) { buffer.assign ((size_t) size, 0.0f); position = 0; }

        float process (float in, int delay)
        {
            const int size = (int) buffer.size();
            buffer[(size_t) position] = in;
            int read = position - delay;
            if (read < 0)
                read += size;
            const float out = buffer[(size_t) read];
            if (++position >= size)
                position = 0;
            return out;
        }
    };

    using Multiplicative = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;
    using Linear = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    void processChunk (float* left, float* right, int numSamples, const Settings&);
    void snapSmoothers (const Settings&);
    void resetSaturation();

    double sampleRate = 48000.0;
    int maxBlock = 512;
    bool needsSnap = true;
    int activeOversampling = 0;
    std::array<int, 4> latencies {};

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;

    std::array<std::vector<float>, 2> dry, delayedDry, band, delayedBand, wetOut;
    std::vector<float> driveGains, biasValues;

    std::array<Delay, 2> dryDelay, bandDelay;
    std::array<woody::Saturator, 2> saturators;
    std::array<woody::DcBlocker, 2> dcBlockers;
    std::array<float, 2> tiltState {};
    juce::dsp::StateVariableTPTFilter<float> lowPass1, lowPass2, highPass1, highPass2;

    Multiplicative inputGain, outputGain, driveGain, toneLow, toneHigh;
    Linear mixAmount, biasAmount, autoAmount;

    double inputMs = 0.0, outputMs = 0.0;
};
} // namespace satwoody
