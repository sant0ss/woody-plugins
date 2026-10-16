#include "Patterns.h"
#include <algorithm>

namespace woody
{
const std::vector<DrumRow>& drumRows()
{
    static const std::vector<DrumRow> rows {
        { "C1",  "Crash 1",          49 },
        { "C2",  "Crash 2",          57 },
        { "R",   "Ride",             51 },
        { "RB",  "Ride Bell",        53 },
        { "FX",  "FX Cymbal",        52 },
        { "HO",  "Hi-Hat Open",      46 },
        { "H",   "Hi-Hat Closed",    42 },
        { "HP",  "Hi-Hat Pedal",     44 },
        { "T1",  "Tom 1",            48 },
        { "T2",  "Tom 2",            45 },
        { "FT",  "Floor Tom",        41 },
        { "S",   "Snare",            38 },
        { "SR",  "Snare Rim",        40 },
        { "SS",  "Side Stick",       37 },
        { "K",   "Kick",             36 },
        { "P1",  "Perc 1",           39 },
        { "P2",  "Perc 2",           54 },
        { "P3",  "Perc 3",           56 },
        { "P4",  "Perc 4",           70 },
        { "HW",  "Hi-Hat Swish",     26 },
        { "RE",  "Ride Edge",        59 },
        { "T1R", "Tom 1 Rim",        50 },
        { "T2R", "Tom 2 Rim",        47 },
        { "FTR", "Floor Tom Rim",    43 },
        { "C1X", "Crash 1 Choke",    28 },
        { "C2X", "Crash 2 Choke",    29 },
        { "RX",  "Ride Choke",       30 },
        { "FXX", "FX Choke",         31 },
    };
    return rows;
}

const DrumRow* rowForCode (const juce::String& code)
{
    for (const auto& r : drumRows())
        if (code == r.code)
            return &r;
    return nullptr;
}

const DrumRow* rowForNote (int note)
{
    for (const auto& r : drumRows())
        if (r.note == note)
            return &r;
    return nullptr;
}

//==============================================================================
int Pattern::velocityAt (int step, int note) const
{
    for (const auto& h : hits)
        if (h.step == step && h.note == note)
            return h.velocity;
    return 0;
}

void Pattern::setHit (int step, int note, int velocity)
{
    hits.erase (std::remove_if (hits.begin(), hits.end(), [&] (const Hit& h) { return h.step == step && h.note == note; }),
                hits.end());

    if (velocity > 0 && step >= 0 && step < totalSteps())
        hits.push_back ({ step, note, juce::jlimit (1, 127, velocity) });
}

void Pattern::resize (int newBars, int newStepsPerBar)
{
    std::vector<Hit> kept;
    for (const auto& h : hits)
    {
        // Map by musical position, so changing the grid keeps what lands on it
        const double beat = stepBeat (h.step);
        const double newStep = beat * newStepsPerBar / 4.0;
        const int rounded = juce::roundToInt (newStep);
        if (std::abs (newStep - rounded) < 1.0e-6 && rounded < newBars * newStepsPerBar)
            kept.push_back ({ rounded, h.note, h.velocity });
    }

    bars = newBars;
    stepsPerBar = newStepsPerBar;
    hits = std::move (kept);
}

juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree tree ("PATTERN");
    tree.setProperty ("name", name, nullptr);
    tree.setProperty ("genre", genre, nullptr);
    tree.setProperty ("bars", bars, nullptr);
    tree.setProperty ("steps", stepsPerBar, nullptr);

    juce::StringArray encoded;
    for (const auto& h : hits)
        encoded.add (juce::String (h.step) + ":" + juce::String (h.note) + ":" + juce::String (h.velocity));
    tree.setProperty ("hits", encoded.joinIntoString (" "), nullptr);
    return tree;
}

Pattern Pattern::fromValueTree (const juce::ValueTree& tree)
{
    Pattern p;
    p.name = tree.getProperty ("name").toString();
    p.genre = tree.getProperty ("genre").toString();
    p.bars = juce::jlimit (1, 8, (int) tree.getProperty ("bars", 1));
    p.stepsPerBar = juce::jlimit (4, 32, (int) tree.getProperty ("steps", 16));

    for (const auto& token : juce::StringArray::fromTokens (tree.getProperty ("hits").toString(), " ", ""))
    {
        const auto parts = juce::StringArray::fromTokens (token, ":", "");
        if (parts.size() == 3)
            p.hits.push_back ({ parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getIntValue() });
    }
    return p;
}

//==============================================================================
namespace
{
double swungBeat (const Pattern& p, int step, float swing)
{
    const int stepsPerBeat = p.stepsPerBar / 4;
    double beat = p.stepBeat (step);

    // Off-beat 16ths (or 8ths) move towards the last triplet of their pair
    if ((stepsPerBeat == 4 || stepsPerBeat == 2) && step % 2 == 1)
        beat += swing * (stepsPerBeat == 4 ? 1.0 / 12.0 : 1.0 / 6.0);

    return beat;
}

void appendPattern (Sequence& seq, const Pattern& p, double offset, float swing)
{
    for (const auto& h : p.hits)
        seq.events.push_back ({ offset + swungBeat (p, h.step, swing), h.note, h.velocity });
}

void sortEvents (Sequence& seq)
{
    std::stable_sort (seq.events.begin(), seq.events.end(),
                      [] (const SeqEvent& a, const SeqEvent& b) { return a.beat < b.beat; });
}
} // namespace

Sequence makeSequence (const Pattern& p, float swing)
{
    Sequence seq;
    seq.lengthBeats = p.lengthBeats();
    appendPattern (seq, p, 0.0, swing);
    sortEvents (seq);
    return seq;
}

Sequence makeSequence (const std::vector<SongPart>& parts, float swing)
{
    Sequence seq;
    double offset = 0.0;

    for (const auto& part : parts)
    {
        for (int r = 0; r < part.repeats; ++r)
        {
            appendPattern (seq, part.pattern, offset, swing);
            offset += part.pattern.lengthBeats();
        }
    }

    seq.lengthBeats = juce::jmax (4.0, offset);
    sortEvents (seq);
    return seq;
}

juce::File writeMidiFile (const Sequence& seq, const juce::String& name)
{
    constexpr int ticksPerBeat = 960;

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    for (const auto& e : seq.events)
    {
        const double start = e.beat * ticksPerBeat;
        track.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.velocity), start);
        track.addEvent (juce::MidiMessage::noteOff (1, e.note), start + ticksPerBeat / 8.0);
    }

    // An end marker keeps the clip exactly the pattern's length
    track.addEvent (juce::MidiMessage::endOfTrack(), seq.lengthBeats * ticksPerBeat);
    track.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerBeat);
    midi.addTrack (track);

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Drum Woodys");
    folder.createDirectory();

    auto safeName = juce::File::createLegalFileName (name.isEmpty() ? juce::String ("Drum Woodys") : name);
    auto file = folder.getChildFile (safeName + ".mid");
    file.deleteFile();

    juce::FileOutputStream out (file);
    if (out.openedOk())
        midi.writeTo (out);

    return file;
}

//==============================================================================
juce::File UserPatterns::file()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Woody Plugins")
        .getChildFile ("Drum Woodys Patterns.xml");
}

std::vector<Pattern> UserPatterns::load()
{
    std::vector<Pattern> patterns;
    if (auto xml = juce::XmlDocument::parse (file()))
    {
        const auto tree = juce::ValueTree::fromXml (*xml);
        for (const auto& child : tree)
            patterns.push_back (Pattern::fromValueTree (child));
    }
    return patterns;
}

void UserPatterns::save (const std::vector<Pattern>& patterns)
{
    juce::ValueTree tree ("USER_PATTERNS");
    for (const auto& p : patterns)
        tree.appendChild (p.toValueTree(), nullptr);

    auto f = file();
    f.getParentDirectory().createDirectory();
    if (auto xml = tree.createXml())
        xml->writeTo (f);
}
} // namespace woody
