#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DrumEngine.h"
#include "Patterns.h"
#include "Presets.h"

class DrumWoodysProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster
{
public:
    DrumWoodysProcessor();
    ~DrumWoodysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::String paramId (int slot, const char* name);

    //==============================================================================
    // Kit (message thread)
    std::shared_ptr<const woody::Library> getLibrary() const { return library; }
    void setLibraryFolder (const juce::File& folder);
    juce::String getSlotInstrument (int slot) const { return selection[(size_t) slot]; }
    void setSlotInstrument (int slot, const juce::String& instrumentId);

    // Kit presets
    void applyFactoryPreset (const woody::FactoryPreset&);
    void applyUserPreset (const juce::ValueTree&);
    juce::ValueTree capturePreset (const juce::String& name) const;
    juce::String currentPresetName;

    //==============================================================================
    // Groove player (message thread)
    void play (const woody::Sequence&, const juce::String& sourceId);
    void updateSequence (const woody::Sequence&, const juce::String& sourceId); // only if that source is playing
    void stop();
    bool isPlaying() const { return previewing.load(); }
    juce::String getPlayingSource() const;
    double getPlayPosition() const { return playPosition.load(); } // beats into the sequence, -1 when silent
    double getHostBpm() const { return hostBpm.load(); }
    float getTiming() const { return timingParam->load(); }

    // Song and step editor content, saved with the project
    std::vector<woody::SongPart> song;
    woody::Pattern editPattern;
    int editorWidth = 0; // last editor size, 0 = default

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState; // pads in the editor play through this
    woody::DrumEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties createBuses();
    woody::MixSettings readMixSettings() const;
    std::unique_ptr<juce::PropertiesFile> openSettings() const;
    void rebuildKit();
    void renderSequence (juce::MidiBuffer& midi, int numSamples);
    void setParameter (const juce::String& id, float value);
    bool isKitParameter (const juce::String& id) const;

    struct StripParams
    {
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* room = nullptr;
        std::atomic<float>* overhead = nullptr;
        std::atomic<float>* tune = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* out = nullptr;
        std::atomic<float>* eqLow = nullptr;
        std::atomic<float>* eqMid = nullptr;
        std::atomic<float>* eqFreq = nullptr;
        std::atomic<float>* eqHigh = nullptr;
        std::atomic<float>* comp = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* drive = nullptr;
    };

    std::array<StripParams, woody::numSlots> stripParams;
    std::atomic<float>* masterParam = nullptr;
    std::atomic<float>* overheadParam = nullptr;
    std::atomic<float>* overheadMuteParam = nullptr;
    std::atomic<float>* roomParam = nullptr;
    std::atomic<float>* roomMuteParam = nullptr;
    std::atomic<float>* roomSizeParam = nullptr;
    std::atomic<float>* roomCompParam = nullptr;
    std::atomic<float>* glueParam = nullptr;
    std::atomic<float>* humanizeParam = nullptr;
    std::atomic<float>* variationParam = nullptr;
    std::atomic<float>* timingParam = nullptr;
    std::atomic<float>* followHostParam = nullptr;

    std::shared_ptr<const woody::Library> library;
    woody::SampleCache sampleCache;
    std::array<juce::String, woody::numSlots> selection;

    // Message-side ownership; the audio thread only sees raw pointers swapped under audioLock
    std::shared_ptr<const woody::Kit> kit;
    std::shared_ptr<const woody::Sequence> sequence;
    juce::String sequenceSource;
    mutable juce::CriticalSection messageLock;

    juce::SpinLock audioLock;
    const woody::Kit* audioKit = nullptr;
    const woody::Sequence* audioSequence = nullptr;
    bool resetVoices = false;

    std::atomic<bool> previewing { false };
    std::atomic<bool> restartClock { false };
    std::atomic<double> playPosition { -1.0 };
    std::atomic<double> hostBpm { 120.0 };
    double internalBeat = 0.0;
    double currentSampleRate = 44100.0;
    juce::Random timingRandom;
    std::array<juce::AudioBuffer<float>, woody::numSlots> auxViews;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumWoodysProcessor)
};
