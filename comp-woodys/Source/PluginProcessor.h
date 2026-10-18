#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "CompEngine.h"
#include "CompParams.h"

class CompProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    CompProcessor();
    ~CompProcessor() override;

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
    const compwoody::CompEngine& getEngine() const { return engine; }
    double getCurrentSampleRate() const { return sampleRate > 0.0 ? sampleRate : 48000.0; }
    float getValue (const juce::String& id) const;
    void setValue (const juce::String& id, float realValue); // notifies the host

    // Peak since the editor last asked
    float takeInputPeak() { return inputPeak.exchange (0.0f); }
    float takeOutputPeak() { return outputPeak.exchange (0.0f); }

    int editorWidth = 0;

    juce::AudioProcessorValueTreeState parameters;

private:
    void timerCallback() override;

    compwoody::ParamSet paramSet;
    compwoody::CompEngine engine;
    double sampleRate = 0.0;
    int latency = 0;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompProcessor)
};
