#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "Dsp.h"
#include "Library.h"

namespace woody
{
// Mixer and effect values read by the audio thread every block.
struct MixSettings
{
    struct Strip
    {
        float levelDb = 0.0f;
        float pan = 0.0f;
        float room = 0.0f;      // send to the room
        float overhead = 0.0f;  // send to the overheads
        float tune = 0.0f;      // semitones
        bool mute = false;
        bool solo = false;
        bool separateOut = false;

        float eqLow = 0.0f, eqMid = 0.0f, eqMidFreq = 1000.0f, eqHigh = 0.0f; // dB, Hz
        float comp = 0.0f;      // 0..1
        float attack = 0.0f;    // -1..1
        float sustain = 0.0f;   // -1..1
        float drive = 0.0f;     // 0..1
    };

    std::array<Strip, numSlots> strips;
    float overheadDb = -6.0f;
    float roomDb = -8.0f;
    bool overheadMute = false;
    bool roomMute = false;
    float roomSize = 0.4f;
    float roomComp = 0.3f;
    float glue = 0.2f;
    float masterDb = 0.0f;
    float humanize = 0.3f;  // gain and pitch variation per hit
    float variation = 0.5f; // round-robin style variation of repeated hits
};

// Levels for the meters, written by the audio thread.
struct Meters
{
    std::array<std::atomic<float>, numSlots> slots {};
    std::atomic<float> overhead { 0.0f }, room { 0.0f }, master { 0.0f };
};

// Plays a Kit: one-shot voices with velocity layers, variations and chokes, each slot
// through its own effects, mixed with overhead and room buses and a master glue/limiter.
class DrumEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // aux[s] is the slot's separate output, or nullptr when that bus is off.
    void process (juce::AudioBuffer<float>& main, const std::array<juce::AudioBuffer<float>*, numSlots>& aux,
                  const juce::MidiBuffer& midi, const Kit* kit, const MixSettings& mix);

    // Incremented on every hit per slot, read by the editor to flash pads.
    std::array<std::atomic<int>, numSlots> hitCounters {};
    Meters meters;

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

    struct SlotFx
    {
        dsp::Biquad low, mid, high;
        dsp::Compressor comp;
        dsp::TransientShaper transient;
        float lastLow = 1.0e9f, lastMid = 1.0e9f, lastMidFreq = 0.0f, lastHigh = 1.0e9f, lastComp = -1.0f;
    };

    void noteOn (const Kit& kit, int note, int velocity, const MixSettings& mix);
    void chokeSlot (int slot);
    void renderVoices (int startSample, int numSamples);
    void processSlotFx (int slot, int numSamples, const MixSettings::Strip&);
    void mixChunk (juce::AudioBuffer<float>& main, const std::array<juce::AudioBuffer<float>*, numSlots>& aux,
                   int startSample, int numSamples, const MixSettings& mix);
    Voice& allocateVoice();

    double sampleRate = 44100.0;
    std::array<Voice, maxVoices> voices;
    std::array<juce::AudioBuffer<float>, numSlots> slotBuffers;
    std::array<SlotFx, numSlots> slotFx;
    juce::AudioBuffer<float> roomBuffer, overheadBuffer, mainChunk;
    juce::Reverb reverb;
    dsp::Compressor roomCompressor, glueCompressor;
    dsp::Biquad overheadLowCut, overheadAir;
    juce::Random random;
    std::array<std::array<int, maxArticulations>, numSlots> roundRobin {};
    std::array<std::array<int, maxArticulations>, numSlots> lastVariant {};
    juce::uint32 voiceAge = 0;
    float lastRoomComp = -1.0f, lastGlue = -1.0f;

    // Gains at the end of the previous chunk, ramped to avoid zipper noise
    std::array<float, numSlots> lastL {}, lastR {}, lastRoomSend {}, lastOverheadSend {};
    float lastMaster = 1.0f, lastRoom = 0.0f, lastOverhead = 0.0f;
    std::array<float, numSlots> blockPeaks {};
    float overheadPeak = 0.0f, roomPeak = 0.0f, masterPeak = 0.0f;
};
} // namespace woody
