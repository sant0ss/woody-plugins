#include "SfzParser.h"
#include <map>

namespace woody
{
namespace
{
using Opcodes = std::map<juce::String, juce::String>;

bool isIdentChar (juce::juce_wchar c)
{
    return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
}

// True when an "identifier=" starts at index i.
bool opcodeStartsAt (const juce::String& s, int i)
{
    const int len = s.length();
    if (i >= len || ! isIdentChar (s[i]))
        return false;

    while (i < len && isIdentChar (s[i]))
        ++i;

    return i < len && s[i] == '=';
}

// Splits one comment-free SFZ line into headers and opcodes. Opcode values may
// contain spaces (sample paths), so a value runs until the next header or opcode.
template <typename OnHeader, typename OnOpcode>
void parseLine (const juce::String& line, OnHeader&& onHeader, OnOpcode&& onOpcode)
{
    const int len = line.length();
    int i = 0;

    while (i < len)
    {
        if (juce::CharacterFunctions::isWhitespace (line[i]))
        {
            ++i;
            continue;
        }

        if (line[i] == '<')
        {
            const int close = line.indexOfChar (i, '>');
            if (close < 0)
                return;

            onHeader (line.substring (i + 1, close).trim().toLowerCase());
            i = close + 1;
            continue;
        }

        if (! opcodeStartsAt (line, i))
        {
            while (i < len && ! juce::CharacterFunctions::isWhitespace (line[i]))
                ++i;
            continue;
        }

        const int eq = line.indexOfChar (i, '=');
        const auto key = line.substring (i, eq).toLowerCase();
        const int valueStart = eq + 1;
        int valueEnd = len;

        for (int k = valueStart; k < len; ++k)
        {
            if (line[k] == '<')
            {
                valueEnd = k;
                break;
            }

            if (juce::CharacterFunctions::isWhitespace (line[k]))
            {
                int m = k;
                while (m < len && juce::CharacterFunctions::isWhitespace (line[m]))
                    ++m;

                if (m >= len || line[m] == '<' || opcodeStartsAt (line, m))
                {
                    valueEnd = k;
                    break;
                }
            }
        }

        onOpcode (key, line.substring (valueStart, valueEnd).trim());
        i = valueEnd;
    }
}

// SFZ keys are MIDI numbers or note names (c4 = 60).
int parseKey (const juce::String& value, int fallback)
{
    const auto v = value.trim().toLowerCase();
    if (v.isEmpty())
        return fallback;

    if (juce::CharacterFunctions::isDigit (v[0]) || v[0] == '-')
        return juce::jlimit (0, 127, v.getIntValue());

    static const int semitones[] = { 9, 11, 0, 2, 4, 5, 7 }; // a b c d e f g
    if (v[0] < 'a' || v[0] > 'g')
        return fallback;

    int note = semitones[v[0] - 'a'];
    int pos = 1;

    if (pos < v.length() && v[pos] == '#') { ++note; ++pos; }
    else if (pos < v.length() && v[pos] == 'b') { --note; ++pos; }

    const int octave = v.substring (pos).getIntValue();
    return juce::jlimit (0, 127, (octave + 1) * 12 + note);
}

const juce::String* find (const Opcodes& ops, const char* key)
{
    const auto it = ops.find (key);
    return it != ops.end() ? &it->second : nullptr;
}
} // namespace

std::vector<SfzRegion> parseSfz (const juce::File& sfzFile)
{
    std::vector<SfzRegion> regions;
    Opcodes control, global, master, group, region;
    Opcodes* current = nullptr;
    bool inRegion = false;

    auto flushRegion = [&]
    {
        if (! inRegion)
            return;

        inRegion = false;

        Opcodes ops = global;
        for (auto& [k, v] : master) ops[k] = v;
        for (auto& [k, v] : group)  ops[k] = v;
        for (auto& [k, v] : region) ops[k] = v;

        const auto* sample = find (ops, "sample");
        if (sample == nullptr || sample->startsWithChar ('*')) // generators like *sine are not supported
            return;

        if (const auto* trigger = find (ops, "trigger"); trigger != nullptr && *trigger != "attack")
            return;

        juce::String relative = sample->replaceCharacter ('\\', '/');
        if (const auto* defaultPath = find (control, "default_path"))
            relative = defaultPath->replaceCharacter ('\\', '/') + relative;

        SfzRegion r;
        r.sample = sfzFile.getParentDirectory().getChildFile (relative);

        if (const auto* v = find (ops, "key"))   r.loKey = r.hiKey = parseKey (*v, 60);
        if (const auto* v = find (ops, "lokey")) r.loKey = parseKey (*v, r.loKey);
        if (const auto* v = find (ops, "hikey")) r.hiKey = parseKey (*v, r.hiKey);
        if (const auto* v = find (ops, "lovel")) r.loVel = v->getIntValue();
        if (const auto* v = find (ops, "hivel")) r.hiVel = v->getIntValue();
        if (const auto* v = find (ops, "volume")) r.gainDb = v->getFloatValue();
        if (const auto* v = find (ops, "tune"))  r.tuneCents += v->getFloatValue();
        if (const auto* v = find (ops, "transpose")) r.tuneCents += v->getFloatValue() * 100.0f;
        if (const auto* v = find (ops, "seq_length"))   r.seqLength = juce::jmax (1, v->getIntValue());
        if (const auto* v = find (ops, "seq_position")) r.seqPosition = juce::jmax (1, v->getIntValue());
        if (const auto* v = find (ops, "lorand")) r.loRand = v->getFloatValue();
        if (const auto* v = find (ops, "hirand")) r.hiRand = v->getFloatValue();

        regions.push_back (r);
    };

    for (auto line : juce::StringArray::fromLines (sfzFile.loadFileAsString()))
    {
        const int slashes = line.indexOf ("//");
        if (slashes >= 0)
            line = line.substring (0, slashes);

        if (line.trimStart().startsWithChar ('#')) // #define / #include are not supported
            continue;

        parseLine (line,
            [&] (const juce::String& header)
            {
                flushRegion();

                if (header == "region")       { region.clear(); current = &region; inRegion = true; }
                else if (header == "group")   { group.clear(); current = &group; }
                else if (header == "master")  { master.clear(); group.clear(); current = &master; }
                else if (header == "global")  { global.clear(); master.clear(); group.clear(); current = &global; }
                else if (header == "control") { current = &control; }
                else current = nullptr; // <curve>, <effect>, ... are ignored
            },
            [&] (const juce::String& key, const juce::String& value)
            {
                if (current != nullptr)
                    (*current)[key] = value;
            });
    }

    flushRegion();
    return regions;
}
} // namespace woody
