#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace woody
{
// A row of the step editor / pattern text: one articulation of one slot.
struct DrumRow
{
    const char* code; // used in the built-in pattern text
    const char* name;
    int note;
};

// Rows in the order the step editor shows them (cymbals on top, kick at the bottom).
const std::vector<DrumRow>& drumRows();
const DrumRow* rowForCode (const juce::String& code);
const DrumRow* rowForNote (int note);

struct Hit
{
    int step = 0;
    int note = 36;
    int velocity = 100;
};

// A drum pattern on a step grid, always in 4/4.
struct Pattern
{
    juce::String name;
    juce::String genre;
    int bars = 1;
    int stepsPerBar = 16; // 16 = 16ths, 12 = 8th-note triplets
    std::vector<Hit> hits;

    int totalSteps() const { return bars * stepsPerBar; }
    double lengthBeats() const { return bars * 4.0; }
    double stepBeat (int step) const { return step * 4.0 / stepsPerBar; }

    int velocityAt (int step, int note) const; // 0 when empty
    void setHit (int step, int note, int velocity); // velocity 0 removes
    void resize (int newBars, int newStepsPerBar);  // keeps hits that still fit

    juce::ValueTree toValueTree() const;
    static Pattern fromValueTree (const juce::ValueTree&);
};

// One block of a song: a pattern played a number of times.
struct SongPart
{
    Pattern pattern;
    int repeats = 1;
};

// Ready-to-play notes, measured in beats (quarter notes).
struct SeqEvent
{
    double beat;
    int note;
    int velocity;
};

struct Sequence
{
    std::vector<SeqEvent> events; // sorted by beat
    double lengthBeats = 4.0;
};

// swing 0..1 delays off-beat 16ths (or 8ths) up to a triplet feel.
Sequence makeSequence (const Pattern&, float swing);
Sequence makeSequence (const std::vector<SongPart>&, float swing);

// Writes a .mid file to the temp folder and returns it, ready to be dragged into the DAW.
// timing 0..1 pushes notes slightly early or late, like a human drummer.
juce::File writeMidiFile (const Sequence&, const juce::String& name, float timing = 0.0f);

// 0 simple, 1 medium, 2 busy
int complexityOf (const Pattern&);

// Typical tempo range of a genre, e.g. "90-140 BPM"
juce::String suggestedTempo (const juce::String& genre);

// A new take on a groove: ghost notes, kick pickups, open hats... Different for every seed.
Pattern makeVariation (const Pattern&, int seed);

// The groove with a fill on its last one or two beats.
Pattern makeFill (const Pattern&, int seed);

// Grooves that ship with the plugin.
const std::vector<Pattern>& builtInPatterns();
juce::StringArray builtInGenres();

// Patterns the user saved, shared by every instance of the plugin.
class UserPatterns
{
public:
    static std::vector<Pattern> load();
    static void save (const std::vector<Pattern>&);

private:
    static juce::File file();
};
} // namespace woody
