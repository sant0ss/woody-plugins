#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "Library.h"

namespace woody
{
// Mixer values read by the audio thread every block.
struct MixSettings
{
    struct Strip
    {
        float levelDb = 0.0f;
        float pan = 0.0f;
        float room = 0.0f;
        float tune = 0.0f; // semitones
        bool mute = false;
        bool solo = false;
    };

    std::array<Strip, numSlots> strips;
    float masterDb = 0.0f;
    float ambience = 0.5f; // room return level
    float roomSize = 0.4f;
    float humanize = 0.3f;
};

// Plays a Kit: one-shot voices with velocity layers, round robins and chokes,
// summed through a per-slot mixer with a shared room reverb.
class DrumEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // Renders one block. midi holds the block's note events.
    void process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi, const Kit* kit, const MixSettings& mix);

    // Incremented on every hit per slot, read by the editor to flash pads.
    std::array<std::atomic<int>, numSlots> hitCounters {};

private:
    static constexpr int maxVoices = 128;
    static constexpr int chunkSize = 256;

    struct Voice
    {
        const SampleData* sample = nullptr;
        int slot = 0;
        double position = 0.0;
        double increment = 1.0;
        float gain = 1.0f;
        float fade = 1.0f, fadeStep = 0.0f;
        juce::uint32 age = 0;
        bool active = false;
    };

    void noteOn (const Kit& kit, int note, int velocity, const MixSettings& mix);
    void chokeSlot (int slot);
    void renderVoices (int startSample, int numSamples);
    void mixChunk (juce::AudioBuffer<float>& output, int startSample, int numSamples, const MixSettings& mix);
    Voice& allocateVoice();

    double sampleRate = 44100.0;
    std::array<Voice, maxVoices> voices;
    std::array<juce::AudioBuffer<float>, numSlots> slotBuffers;
    juce::AudioBuffer<float> roomBuffer;
    juce::Reverb reverb;
    juce::Random random;
    std::array<std::array<int, maxArticulations>, numSlots> roundRobin {};
    juce::uint32 voiceAge = 0;

    // Gains at the end of the previous chunk, ramped to avoid zipper noise
    std::array<float, numSlots> lastL {}, lastR {}, lastSend {};
    float lastMaster = 1.0f, lastAmbience = 0.0f;
};
} // namespace woody
