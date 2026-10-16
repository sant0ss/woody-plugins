#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <vector>
#include "SfzParser.h"
#include "Slots.h"

namespace woody
{
// A kit piece found in the library, e.g. "Black Pearl - Pearl 14 Snare".
// Its articulations follow the order of the slot type in slotDefs().
struct Instrument
{
    juce::String id;      // stable id saved in projects
    juce::String name;    // shown to the user
    juce::String kitName;
    SlotType type = percType;
    int firstKey = 0;     // for sorting
    std::array<std::vector<SfzRegion>, maxArticulations> articulations;
};

// Every instrument from every .sfz kit inside the library folder.
class Library
{
public:
    static std::shared_ptr<const Library> scan (const juce::File& folder);

    const juce::File& getFolder() const { return folder; }
    const std::vector<Instrument>& getInstruments() const { return instruments; }
    const Instrument* find (const juce::String& id) const;
    std::vector<const Instrument*> instrumentsFor (int slot) const;

    // What a fresh kit uses for this slot ("" when nothing fits)
    juce::String defaultFor (int slot) const;

private:
    juce::File folder;
    std::vector<Instrument> instruments;
};

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

// Loads each audio file once and shares it between kits.
class SampleCache
{
public:
    std::shared_ptr<const SampleData> get (const juce::File& file);

private:
    juce::CriticalSection lock;
    juce::AudioFormatManager formats;
    std::map<juce::String, std::weak_ptr<const SampleData>> cache;
    bool formatsRegistered = false;
};

// What the audio thread plays: the chosen instrument of each slot, with its audio loaded.
struct KitLayer
{
    std::shared_ptr<const SampleData> sample;
    int loVel = 1, hiVel = 127;
    float gain = 1.0f;
    float tuneCents = 0.0f;
    int seqLength = 1, seqPosition = 1;
    float loRand = 0.0f, hiRand = 1.0f;
};

struct KitSlot
{
    juce::String instrumentId;
    std::array<std::vector<KitLayer>, maxArticulations> articulations;
};

struct Kit
{
    std::array<KitSlot, numSlots> slots;

    static std::shared_ptr<const Kit> build (const Library&, const std::array<juce::String, numSlots>& selection, SampleCache&);
};
} // namespace woody
