#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SatEngine.h"
#include "SatParams.h"

class SatProcessor : public juce::AudioProcessor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::Timer
{
public:
    SatProcessor();
    ~SatProcessor() override;

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

    float getValue (const juce::String& id) const;
    int getLatency() const { return latency.load(); }

    satwoody::SatEngine engine;
    int editorWidth = 0;

    juce::AudioProcessorValueTreeState parameters;

private:
    void parameterChanged (const juce::String&, float) override { latencyDirty = true; }
    void timerCallback() override;
    satwoody::Settings readSettings() const;

    std::atomic<float> *inParam, *driveParam, *typeParam, *toneParam, *biasParam, *mixParam, *outParam,
                       *autoParam, *stereoParam, *focusLowParam, *focusHighParam, *osParam, *bypassParam;

    std::atomic<bool> latencyDirty { true };
    std::atomic<int> latency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatProcessor)
};
