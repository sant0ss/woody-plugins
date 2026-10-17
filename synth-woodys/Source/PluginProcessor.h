#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Effects.h"
#include "Params.h"
#include "SynthEngine.h"

class SynthWoodysProcessor : public juce::AudioProcessor
{
public:
    SynthWoodysProcessor();
    ~SynthWoodysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Presets (message thread)
    void applyFactoryPreset (const synthwoody::FactoryPreset&);
    void applyUserPreset (const juce::ValueTree&);
    juce::ValueTree capturePreset (const juce::String& name) const;
    static std::vector<juce::ValueTree> loadUserPresets();
    static void saveUserPresets (const std::vector<juce::ValueTree>&);

    void panic() { panicRequested = true; }
    float getOutputPeak() const { return outputPeak.load(); }

    juce::String currentPresetName;
    int editorWidth = 0;

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

private:
    void setParameter (const juce::String& id, float value);
    float get (synthwoody::ParamIndex index) const { return raw[(size_t) index]->load(); }
    synthwoody::Settings readSettings() const;
    synthwoody::FxSettings readFxSettings() const;

    std::array<std::atomic<float>*, synthwoody::numParams> raw {};
    synthwoody::Synth synth;
    synthwoody::Effects effects;

    std::atomic<bool> panicRequested { false };
    std::atomic<float> outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthWoodysProcessor)
};
