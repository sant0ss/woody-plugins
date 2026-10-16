#include "Library.h"

namespace woody
{
namespace
{
struct Classification
{
    SlotType type;
    int articulation;
};

// Kit pieces are recognised by the last word of their sample names,
// e.g. "38-Pearl-14-SnareEdge-3.wav" -> snare rim.
Classification classify (const juce::String& piece)
{
    const auto p = piece.toLowerCase();
    const bool choke = p.contains ("choke");

    if (p.contains ("kick"))                                        return { kickType, 0 };
    if (p.contains ("sidestick"))                                   return { snareType, 2 };
    if (p.contains ("snareedge") || p.contains ("rimshot"))         return { snareType, 1 };
    if (p.contains ("snare"))                                       return { snareType, 0 };
    if (p.contains ("hatpedal"))                                    return { hihatType, 1 };
    if (p.contains ("hatsemi") || p.contains ("hatopen"))           return { hihatType, 2 };
    if (p.contains ("hatswish"))                                    return { hihatType, 3 };
    if (p.contains ("hat"))                                         return { hihatType, 0 };
    if (p.contains ("tom"))                                         return { tomType, p.contains ("edge") || p.contains ("rim") ? 1 : 0 };
    if (p.contains ("china") || p.contains ("splash"))              return { fxType, choke ? 1 : 0 };
    if (p.contains ("crash"))                                       return { crashType, choke ? 1 : 0 };
    if (p.contains ("ride"))
    {
        if (choke)                                                  return { rideType, 3 };
        if (p.contains ("bell"))                                    return { rideType, 1 };
        if (p.contains ("shank") || p.contains ("edge"))            return { rideType, 2 };
        return { rideType, 0 };
    }

    return { percType, 0 };
}

// "HandClap" -> "Hand Clap"
juce::String splitCamel (const juce::String& s)
{
    juce::String out;
    for (int i = 0; i < s.length(); ++i)
    {
        const auto c = s[i];
        if (i > 0 && juce::CharacterFunctions::isUpperCase (c) && juce::CharacterFunctions::isLowerCase (s[i - 1]))
            out << ' ';
        out << juce::String::charToString (c);
    }
    return out;
}

// "BLONDE_BOP_HR" -> "Blonde Bop HR", "Black_Pearl_2023_repack" -> "Black Pearl"
juce::String cleanKitName (const juce::String& fileName)
{
    juce::StringArray words;
    for (const auto& token : juce::StringArray::fromTokens (fileName, "_ ", ""))
    {
        const auto lower = token.toLowerCase();
        if (lower == "repack" || lower == "sfz" || token.containsOnly ("0123456789"))
            continue;

        if (token.length() > 2 && token == token.toUpperCase())
            words.add (token.substring (0, 1) + token.substring (1).toLowerCase());
        else
            words.add (token);
    }
    return words.joinIntoString (" ");
}

// "38-Pearl-14-SnareEdge-3" -> "Pearl-14-SnareEdge"
juce::String pieceBaseName (const juce::File& sample)
{
    auto name = sample.getFileNameWithoutExtension();

    while (name.isNotEmpty() && juce::CharacterFunctions::isDigit (name.getLastCharacter()))
        name = name.dropLastCharacters (1);
    name = name.trimCharactersAtEnd ("-_ ");

    if (name.endsWithIgnoreCase ("-HR"))
        name = name.dropLastCharacters (3);

    int start = 0;
    while (start < name.length() && juce::CharacterFunctions::isDigit (name[start]))
        ++start;

    return name.substring (start).trimCharactersAtStart ("-_ ");
}

juce::String typeLabel (SlotType type, const juce::String& piece)
{
    switch (type)
    {
        case kickType:  return "Kick";
        case snareType: return "Snare";
        case hihatType: return "Hi-Hat";
        case tomType:   return piece.containsIgnoreCase ("floor") ? "Floor Tom" : "Tom";
        case crashType: return "Crash";
        case rideType:  return "Ride";
        case fxType:    return piece.containsIgnoreCase ("china") ? "China" : "Splash";
        case percType:  break;
    }
    return {};
}
} // namespace

std::shared_ptr<const Library> Library::scan (const juce::File& folder)
{
    auto library = std::make_shared<Library>();
    library->folder = folder;

    auto sfzFiles = folder.findChildFiles (juce::File::findFiles, true, "*.sfz");
    std::sort (sfzFiles.begin(), sfzFiles.end());

    for (const auto& sfz : sfzFiles)
    {
        const auto kitName = cleanKitName (sfz.getFileNameWithoutExtension());
        const auto kitKey = sfz.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
        std::map<juce::String, size_t> indexByKey;

        for (const auto& region : parseSfz (sfz))
        {
            const auto base = pieceBaseName (region.sample);
            auto tokens = juce::StringArray::fromTokens (base, "-", "");
            tokens.removeEmptyStrings();
            if (tokens.isEmpty())
                continue;

            const auto piece = tokens[tokens.size() - 1];
            tokens.remove (tokens.size() - 1);
            const auto model = tokens.joinIntoString (" ");
            const auto c = classify (piece);

            // Drums group by kit, so each kit has one kick/snare/hi-hat; cymbals and toms also by model.
            juce::String key;
            switch (c.type)
            {
                case kickType: case snareType: case hihatType: key = kitKey + "|" + juce::String ((int) c.type); break;
                case percType: key = kitKey + "|perc|" + base; break;
                default:       key = kitKey + "|" + juce::String ((int) c.type) + "|" + model + "|" + typeLabel (c.type, piece); break;
            }

            auto it = indexByKey.find (key);
            if (it == indexByKey.end())
            {
                Instrument inst;
                inst.id = key;
                inst.kitName = kitName;
                inst.type = c.type;
                inst.firstKey = region.loKey;

                const auto label = c.type == percType ? splitCamel (base.replaceCharacter ('-', ' '))
                                                      : (model + " " + typeLabel (c.type, piece)).trim();
                inst.name = kitName + " - " + label;

                library->instruments.push_back (std::move (inst));
                it = indexByKey.emplace (key, library->instruments.size() - 1).first;
            }

            auto& inst = library->instruments[it->second];
            inst.firstKey = juce::jmin (inst.firstKey, region.loKey);
            inst.articulations[(size_t) c.articulation].push_back (region);

            // Name drums after their main articulation's model
            if (c.articulation == 0 && (c.type == snareType || c.type == hihatType) && model.isNotEmpty())
                inst.name = kitName + " - " + model + " " + typeLabel (c.type, piece);
        }
    }

    std::stable_sort (library->instruments.begin(), library->instruments.end(),
                      [] (const Instrument& a, const Instrument& b)
                      {
                          if (a.kitName != b.kitName)
                              return a.kitName < b.kitName;
                          return a.firstKey < b.firstKey;
                      });
    return library;
}

const Instrument* Library::find (const juce::String& id) const
{
    for (const auto& inst : instruments)
        if (inst.id == id)
            return &inst;
    return nullptr;
}

std::vector<const Instrument*> Library::instrumentsFor (int slot) const
{
    std::vector<const Instrument*> result;
    for (const auto& inst : instruments)
        if (inst.type == slotDefs()[(size_t) slot].type)
            result.push_back (&inst);
    return result;
}

juce::String Library::defaultFor (int slot) const
{
    const auto candidates = instrumentsFor (slot);
    if (candidates.empty())
        return {};

    const juce::String mainKit ("Black Pearl");

    auto pick = [&] (auto&& predicate, int skip = 0) -> const Instrument*
    {
        // Main kit first, then any kit
        for (int pass = 0; pass < 2; ++pass)
        {
            int toSkip = skip;
            for (const auto* inst : candidates)
            {
                if (pass == 0 && inst->kitName != mainKit)
                    continue;
                if (! predicate (*inst))
                    continue;
                if (toSkip-- > 0)
                    continue;
                return inst;
            }
        }
        return nullptr;
    };

    auto isFloor = [] (const Instrument& i) { return i.name.containsIgnoreCase ("Floor"); };
    auto any = [] (const Instrument&) { return true; };
    auto named = [] (std::initializer_list<const char*> words)
    {
        return [words] (const Instrument& i)
        {
            for (const auto* w : words)
                if (i.name.containsIgnoreCase (w))
                    return true;
            return false;
        };
    };

    const Instrument* chosen = nullptr;

    switch (slot)
    {
        case tom1:
            // A smaller tom from another kit gives the set three different toms
            for (const auto* inst : candidates)
                if (inst->kitName == "Blonde Bop" && ! isFloor (*inst)) { chosen = inst; break; }
            if (chosen == nullptr) chosen = pick ([&] (const Instrument& i) { return ! isFloor (i); });
            break;
        case tom2:     chosen = pick ([&] (const Instrument& i) { return ! isFloor (i); }); break;
        case floorTom: chosen = pick (isFloor); break;
        case crash2:   chosen = pick (any, 1); break;
        case fxCymbal: chosen = pick (named ({ "China" })); break;
        case perc1:    chosen = pick (named ({ "Clap" })); break;
        case perc2:    chosen = pick (named ({ "Tambourine" })); break;
        case perc3:    chosen = pick (named ({ "Cowbell" })); break;
        case perc4:    chosen = pick (named ({ "Maraca", "Shaker" })); break;
        default:       break;
    }

    if (chosen == nullptr)
        chosen = pick (any);

    return chosen != nullptr ? chosen->id : candidates.front()->id;
}

//==============================================================================
std::shared_ptr<const SampleData> SampleCache::get (const juce::File& file)
{
    const juce::ScopedLock sl (lock);

    if (! formatsRegistered)
    {
        formats.registerBasicFormats();
        formatsRegistered = true;
    }

    const auto key = file.getFullPathName();
    if (auto existing = cache[key].lock())
        return existing;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return {};

    auto data = std::make_shared<SampleData>();
    data->sampleRate = reader->sampleRate;
    data->buffer.setSize (juce::jmin (2, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&data->buffer, 0, (int) reader->lengthInSamples, 0, true, true);

    cache[key] = data;
    return data;
}

//==============================================================================
std::shared_ptr<const Kit> Kit::build (const Library& library, const std::array<juce::String, numSlots>& selection, SampleCache& cache)
{
    auto kit = std::make_shared<Kit>();

    for (int s = 0; s < numSlots; ++s)
    {
        const auto* inst = library.find (selection[(size_t) s]);
        if (inst == nullptr)
            continue;

        auto& slot = kit->slots[(size_t) s];
        slot.instrumentId = inst->id;

        for (int a = 0; a < maxArticulations; ++a)
        {
            for (const auto& region : inst->articulations[(size_t) a])
            {
                auto sample = cache.get (region.sample);
                if (sample == nullptr)
                    continue;

                KitLayer layer;
                layer.sample = std::move (sample);
                layer.loVel = region.loVel;
                layer.hiVel = region.hiVel;
                layer.gain = juce::Decibels::decibelsToGain (region.gainDb);
                layer.tuneCents = region.tuneCents;
                layer.seqLength = region.seqLength;
                layer.seqPosition = region.seqPosition;
                layer.loRand = region.loRand;
                layer.hiRand = region.hiRand;
                slot.articulations[(size_t) a].push_back (std::move (layer));
            }
        }
    }

    return kit;
}
} // namespace woody
