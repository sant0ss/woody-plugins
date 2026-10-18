#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Analyzer.h"
#include "EqEngine.h"
#include "EqParams.h"
#include "LinearPhase.h"

class EqProcessor : public juce::AudioProcessor,
                    private juce::AudioProcessorValueTreeState::Listener,
                    private juce::Timer
{
public:
    EqProcessor();
    ~EqProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Readable from any thread
    eqwoody::BandsArray readBands() const;
    double getCurrentSampleRate() const { return sampleRate > 0.0 ? sampleRate : 48000.0; }
    float getValue (const juce::String& id) const;
    void setValue (const juce::String& id, float realValue); // notifies the host
    void resetAll();

    eqwoody::AnalyzerFifo preFifo, postFifo;
    std::atomic<float> outputPeak { 0.0f };
    int editorWidth = 0;

    juce::AudioProcessorValueTreeState parameters;

private:
    void parameterChanged (const juce::String&, float) override { filterDirty = true; }
    void timerCallback() override;
    void processChunk (float* left, float* right, int numSamples);

    struct DelayLine
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

    double sampleRate = 0.0;
    int maxBlock = 512;

    eqwoody::EqEngine engine;
    eqwoody::LinearPhaseFilter linearFilter;
    std::array<DelayLine, 2> satDelay;
    std::vector<float> satLeft, satRight;

    std::array<std::array<std::atomic<float>*, 9>, eqwoody::numBands> bandParams {};
    std::atomic<float> *outParam = nullptr, *phaseParam = nullptr, *qualityParam = nullptr, *bypassParam = nullptr;

    std::atomic<bool> filterDirty { true };
    std::atomic<bool> needsCross { false };
    std::atomic<int> latency { 0 };
    float lastOut = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqProcessor)
};
