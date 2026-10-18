#pragma once

#include "Compressor.h"
#include "Crossover.h"
#include <array>
#include <atomic>
#include <vector>

namespace compwoody
{
constexpr std::array<int, 4> lookaheadMs { 0, 1, 2, 5 };

struct BandParams
{
    float thresholdDb = -24.0f;
    float ratio = 3.0f;
    float attackMs = 20.0f;
    float releaseMs = 200.0f;
    float makeupDb = 0.0f;
    bool solo = false;
    bool bypass = false;
};

// Everything the engine needs for one block, read from the plugin parameters
struct EngineParams
{
    bool multiband = false;
    bool bypass = false;
    bool midSide = false;

    // Single band
    Compressor::Settings single;
    int lookaheadChoice = 0; // index into lookaheadMs

    // Multiband
    std::array<float, numCrossovers> crossovers { 120.0f, 1000.0f, 5000.0f };
    std::array<BandParams, numBands> bands;
};

// Both modes behind one call: channel mode, bypass, lookahead and the compressor cores
class CompEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // In place, any number of samples (longer blocks are processed in chunks)
    void process (float* left, float* right, int numSamples, const EngineParams&);

    static int latencyFor (const EngineParams&, double sampleRate);

    // Readable from any thread
    float getSingleGainReductionDb() const { return single.getGainReductionDb(); }
    float getBandGainReductionDb (int band) const;
    float getInputLevelDb() const { return single.getInputLevelDb(); }

private:
    void processChunk (float* left, float* right, int numSamples, const EngineParams&);
    void processSingle (float* left, float* right, int numSamples, const EngineParams&);
    void processMultiband (float* left, float* right, int numSamples, const EngineParams&);

    double sampleRate = 48000.0;
    int maxBlock = 512;
    bool wasMultiband = false;
    bool firstBlock = true;
    bool newMode = false; // true for the chunk that starts a mode: parameters jump instead of ramping

    Compressor single;
    std::array<Compressor, numBands> bandCompressors;
    BandSplitter splitter;

    std::array<std::array<std::vector<float>, 2>, numBands> bandBuffers;
    std::array<std::vector<float>, 2> dryBuffers;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, numBands> bandGains;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fadeIn;
    std::array<std::atomic<bool>, numBands> bandBypassed {};
};
} // namespace compwoody
