#include "CompEngine.h"
#include <cmath>

namespace compwoody
{
namespace
{
constexpr int maxLookaheadMs = 5;

int lookaheadSamplesFor (int choice, double sampleRate)
{
    const int ms = lookaheadMs[(size_t) juce::jlimit (0, (int) lookaheadMs.size() - 1, choice)];
    return (int) std::lround ((double) ms * 0.001 * sampleRate);
}

void encodeMidSide (float* left, float* right, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);
        const float side = 0.5f * (left[i] - right[i]);
        left[i] = mid;
        right[i] = side;
    }
}

void decodeMidSide (float* mid, float* side, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float l = mid[i] + side[i];
        const float r = mid[i] - side[i];
        mid[i] = l;
        side[i] = r;
    }
}
} // namespace

void CompEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = juce::jmax (16, maxBlockSize);

    const int maxLookahead = lookaheadSamplesFor ((int) lookaheadMs.size() - 1, sampleRate) + 1;
    single.prepare (sampleRate, maxLookahead);
    for (auto& c : bandCompressors)
        c.prepare (sampleRate, 0);
    splitter.prepare (sampleRate);

    for (auto& band : bandBuffers)
        for (auto& channel : band)
            channel.assign ((size_t) maxBlock, 0.0f);
    for (auto& d : dryBuffers)
        d.assign ((size_t) maxBlock, 0.0f);

    for (auto& g : bandGains)
        g.reset (sampleRate, 0.01);
    bypassAmount.reset (sampleRate, 0.01);
    fadeIn.reset (sampleRate, 0.01);

    reset();
}

void CompEngine::reset()
{
    single.reset();
    for (auto& c : bandCompressors)
        c.reset();
    splitter.reset();
    firstBlock = true;
}

int CompEngine::latencyFor (const EngineParams& p, double sampleRate)
{
    return p.multiband ? 0 : lookaheadSamplesFor (p.lookaheadChoice, sampleRate);
}

float CompEngine::getBandGainReductionDb (int band) const
{
    const auto b = (size_t) juce::jlimit (0, numBands - 1, band);
    return bandBypassed[b].load() ? 0.0f : bandCompressors[b].getGainReductionDb();
}

void CompEngine::process (float* left, float* right, int numSamples, const EngineParams& p)
{
    for (int start = 0; start < numSamples; start += maxBlock)
        processChunk (left + start, right + start, juce::jmin (maxBlock, numSamples - start), p);
}

void CompEngine::processChunk (float* left, float* right, int numSamples, const EngineParams& p)
{
    const bool modeChanged = ! firstBlock && p.multiband != wasMultiband;

    if (modeChanged || firstBlock)
    {
        reset();
        firstBlock = false;

        if (modeChanged)
        {
            fadeIn.setCurrentAndTargetValue (0.0f);
            fadeIn.setTargetValue (1.0f);
        }
        else
            fadeIn.setCurrentAndTargetValue (1.0f);

        bypassAmount.setCurrentAndTargetValue (p.bypass ? 1.0f : 0.0f);
        wasMultiband = p.multiband;
        newMode = true;
    }

    if (p.multiband)
        processMultiband (left, right, numSamples, p);
    else
        processSingle (left, right, numSamples, p);

    newMode = false;

    if (fadeIn.isSmoothing())
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = fadeIn.getNextValue();
            left[i] *= g;
            right[i] *= g;
        }
}

void CompEngine::processSingle (float* left, float* right, int numSamples, const EngineParams& p)
{
    auto settings = p.single;
    settings.lookaheadSamples = lookaheadSamplesFor (p.lookaheadChoice, sampleRate);
    // Bypass keeps the lookahead delay, so switching it never shifts the timing
    if (p.bypass)
        settings.mix = 0.0f;

    single.setSettings (settings);

    if (p.midSide)
        encodeMidSide (left, right, numSamples);

    single.process (left, right, numSamples);

    if (p.midSide)
        decodeMidSide (left, right, numSamples);
}

void CompEngine::processMultiband (float* left, float* right, int numSamples, const EngineParams& p)
{
    float* dry[2] = { dryBuffers[0].data(), dryBuffers[1].data() };
    std::copy (left, left + numSamples, dry[0]);
    std::copy (right, right + numSamples, dry[1]);

    if (p.midSide)
        encodeMidSide (left, right, numSamples);

    splitter.setCrossovers (p.crossovers, newMode);

    float* bandPointers[2][numBands];
    for (int c = 0; c < 2; ++c)
        for (int b = 0; b < numBands; ++b)
            bandPointers[c][b] = bandBuffers[(size_t) b][(size_t) c].data();

    float* const* const channelBands[2] = { bandPointers[0], bandPointers[1] };
    const float* const input[2] = { left, right };
    splitter.split (input, channelBands, numSamples);

    bool anySolo = false;
    for (const auto& band : p.bands)
        anySolo = anySolo || band.solo;

    for (int b = 0; b < numBands; ++b)
    {
        const auto& band = p.bands[(size_t) b];

        Compressor::Settings s;
        s.thresholdDb = band.thresholdDb;
        s.ratio = band.ratio;
        s.attackMs = band.attackMs;
        s.releaseMs = band.releaseMs;
        s.makeupDb = band.makeupDb;
        s.kneeDb = p.single.kneeDb;
        s.link = p.single.link;
        s.rms = p.single.rms;
        s.mix = band.bypass ? 0.0f : p.single.mix;

        auto& compressor = bandCompressors[(size_t) b];
        compressor.setSettings (s);
        compressor.process (bandPointers[0][b], bandPointers[1][b], numSamples);
        bandBypassed[(size_t) b] = band.bypass;

        const float gainTarget = (! anySolo || band.solo) ? 1.0f : 0.0f;
        if (newMode)
            bandGains[(size_t) b].setCurrentAndTargetValue (gainTarget);
        else
            bandGains[(size_t) b].setTargetValue (gainTarget);
    }

    float* io[2] = { left, right };
    for (int c = 0; c < 2; ++c)
        std::fill (io[c], io[c] + numSamples, 0.0f);

    for (int b = 0; b < numBands; ++b)
    {
        auto& gain = bandGains[(size_t) b];
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gain.getNextValue();
            left[i] += bandPointers[0][b][i] * g;
            right[i] += bandPointers[1][b][i] * g;
        }
    }

    if (p.midSide)
        decodeMidSide (left, right, numSamples);

    bypassAmount.setTargetValue (p.bypass ? 1.0f : 0.0f);
    if (bypassAmount.isSmoothing() || bypassAmount.getTargetValue() > 0.5f)
        for (int i = 0; i < numSamples; ++i)
        {
            const float amount = bypassAmount.getNextValue();
            left[i] = amount >= 1.0f ? dry[0][i] : left[i] + amount * (dry[0][i] - left[i]);
            right[i] = amount >= 1.0f ? dry[1][i] : right[i] + amount * (dry[1][i] - right[i]);
        }
}
} // namespace compwoody
