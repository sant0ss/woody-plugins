#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace woody;

namespace
{
juce::String paramId (int slot, const char* name)
{
    return juce::String (slotDefs()[(size_t) slot].id) + "_" + name;
}

constexpr auto libraryFolderKey = "libraryFolder";
} // namespace

DrumWoodysProcessor::DrumWoodysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "DrumWoodys", createLayout())
{
    for (int s = 0; s < numSlots; ++s)
    {
        auto& p = stripParams[(size_t) s];
        p.level = parameters.getRawParameterValue (paramId (s, "level"));
        p.pan   = parameters.getRawParameterValue (paramId (s, "pan"));
        p.room  = parameters.getRawParameterValue (paramId (s, "room"));
        p.tune  = parameters.getRawParameterValue (paramId (s, "tune"));
        p.mute  = parameters.getRawParameterValue (paramId (s, "mute"));
        p.solo  = parameters.getRawParameterValue (paramId (s, "solo"));
    }

    masterParam     = parameters.getRawParameterValue ("master");
    ambienceParam   = parameters.getRawParameterValue ("ambience");
    roomSizeParam   = parameters.getRawParameterValue ("roomSize");
    humanizeParam   = parameters.getRawParameterValue ("humanize");
    followHostParam = parameters.getRawParameterValue ("followHost");

    editPattern.name = "Meu groove";
    editPattern.genre = juce::String::fromUTF8 ("Meus padrões");

    juce::File folder;
    if (auto settings = openSettings())
        folder = juce::File (settings->getValue (libraryFolderKey));

    library = Library::scan (folder);
    resetKitToDefaults();
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumWoodysProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::NormalisableRange<float> dbRange (-60.0f, 6.0f, 0.1f);
    dbRange.setSkewForCentre (-12.0f);

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& def = slotDefs()[(size_t) s];
        const juce::String name (def.name);

        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramId (s, "level"), 1 }, name + " Level", dbRange, 0.0f,
                                                                 juce::AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramId (s, "pan"), 1 }, name + " Pan",
                                                                 juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), def.defaultPan));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramId (s, "room"), 1 }, name + " Room",
                                                                 juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), def.defaultRoom));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramId (s, "tune"), 1 }, name + " Tune",
                                                                 juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                                 juce::AudioParameterFloatAttributes().withLabel ("st")));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { paramId (s, "mute"), 1 }, name + " Mute", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { paramId (s, "solo"), 1 }, name + " Solo", false));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master", dbRange, 0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "ambience", 1 }, "Ambience",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "roomSize", 1 }, "Room Size",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "humanize", 1 }, "Humanize",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "followHost", 1 }, "Follow Host", false));
    return layout;
}

std::unique_ptr<juce::PropertiesFile> DrumWoodysProcessor::openSettings() const
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Drum Woodys";
    options.folderName = "Woody Plugins";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (options);
}

//==============================================================================
void DrumWoodysProcessor::rebuildKit()
{
    std::shared_ptr<const Kit> newKit = Kit::build (*library, selection, sampleCache);
    std::shared_ptr<const Kit> oldKit;
    {
        const juce::ScopedLock sl (messageLock);
        oldKit = kit;
        kit = newKit;
    }
    {
        const juce::SpinLock::ScopedLockType sl (audioLock);
        audioKit = newKit.get();
        resetVoices = true;
    }

    sendChangeMessage();
} // oldKit is released here, after the audio thread stopped using it

void DrumWoodysProcessor::setLibraryFolder (const juce::File& folder)
{
    library = Library::scan (folder);

    if (auto settings = openSettings())
        settings->setValue (libraryFolderKey, folder.getFullPathName());

    // Keep the pieces that still exist
    for (int s = 0; s < numSlots; ++s)
        if (library->find (selection[(size_t) s]) == nullptr)
            selection[(size_t) s] = library->defaultFor (s);

    rebuildKit();
}

void DrumWoodysProcessor::setSlotInstrument (int slot, const juce::String& instrumentId)
{
    selection[(size_t) slot] = instrumentId;
    rebuildKit();
}

void DrumWoodysProcessor::resetKitToDefaults()
{
    for (int s = 0; s < numSlots; ++s)
        selection[(size_t) s] = library->defaultFor (s);

    rebuildKit();
}

//==============================================================================
void DrumWoodysProcessor::play (const Sequence& seq, const juce::String& sourceId)
{
    auto newSeq = std::make_shared<const Sequence> (seq);
    std::shared_ptr<const Sequence> oldSeq;
    {
        const juce::ScopedLock sl (messageLock);
        oldSeq = sequence;
        sequence = newSeq;
        sequenceSource = sourceId;
    }
    {
        const juce::SpinLock::ScopedLockType sl (audioLock);
        audioSequence = newSeq.get();
    }

    restartClock = true;
    previewing = true;
    sendChangeMessage();
}

void DrumWoodysProcessor::updateSequence (const Sequence& seq, const juce::String& sourceId)
{
    if (getPlayingSource() != sourceId)
        return;

    auto newSeq = std::make_shared<const Sequence> (seq);
    std::shared_ptr<const Sequence> oldSeq;
    {
        const juce::ScopedLock sl (messageLock);
        oldSeq = sequence;
        sequence = newSeq;
    }
    const juce::SpinLock::ScopedLockType sl (audioLock);
    audioSequence = newSeq.get();
}

void DrumWoodysProcessor::stop()
{
    previewing = false;
    sendChangeMessage();
}

juce::String DrumWoodysProcessor::getPlayingSource() const
{
    const juce::ScopedLock sl (messageLock);
    return sequenceSource;
}

//==============================================================================
void DrumWoodysProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    keyboardState.reset();
}

bool DrumWoodysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

MixSettings DrumWoodysProcessor::readMixSettings() const
{
    MixSettings mix;

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& p = stripParams[(size_t) s];
        auto& strip = mix.strips[(size_t) s];
        strip.levelDb = p.level->load();
        strip.pan = p.pan->load();
        strip.room = p.room->load();
        strip.tune = p.tune->load();
        strip.mute = p.mute->load() > 0.5f;
        strip.solo = p.solo->load() > 0.5f;
    }

    mix.masterDb = masterParam->load();
    mix.ambience = ambienceParam->load();
    mix.roomSize = roomSizeParam->load();
    mix.humanize = humanizeParam->load();
    return mix;
}

// Adds the groove's notes that fall inside this block. Follows the host's song
// position while it plays, otherwise runs on its own clock at the host tempo.
void DrumWoodysProcessor::renderSequence (juce::MidiBuffer& midi, int numSamples)
{
    bool hostPlaying = false;
    double ppq = 0.0;
    double bpm = 120.0;

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            hostPlaying = position->getIsPlaying();
            if (const auto b = position->getBpm()) bpm = *b;
            if (const auto p = position->getPpqPosition()) ppq = *p;
        }
    }

    hostBpm = bpm;

    if (restartClock.exchange (false))
        internalBeat = 0.0;

    const bool active = previewing.load() || (followHostParam->load() > 0.5f && hostPlaying);
    if (! active || audioSequence == nullptr || audioSequence->lengthBeats <= 0.0)
    {
        playPosition = -1.0;
        return;
    }

    const auto& seq = *audioSequence;
    const double length = seq.lengthBeats;
    const double beatsPerSample = bpm / (60.0 * currentSampleRate);
    const double startBeat = hostPlaying ? ppq : internalBeat;
    const double blockBeats = numSamples * beatsPerSample;

    double pos = std::fmod (startBeat, length);
    if (pos < 0.0)
        pos += length;

    playPosition = pos;

    // Walk the block in segments that stop at the loop end. Each pass consumes
    // a positive amount, so rounding can never stall the loop.
    double remaining = blockBeats;
    double offsetBeats = 0.0;

    while (remaining > 1.0e-9)
    {
        const double segment = juce::jmin (remaining, length - pos);
        const double segmentEnd = pos + segment;

        auto it = std::lower_bound (seq.events.begin(), seq.events.end(), pos,
                                    [] (const SeqEvent& e, double b) { return e.beat < b; });

        for (; it != seq.events.end() && it->beat < segmentEnd; ++it)
        {
            const int offset = juce::jlimit (0, numSamples - 1, (int) ((offsetBeats + it->beat - pos) / beatsPerSample));
            midi.addEvent (juce::MidiMessage::noteOn (1, it->note, (juce::uint8) it->velocity), offset);
        }

        offsetBeats += segment;
        remaining -= segment;
        pos = segmentEnd;
        if (pos >= length - 1.0e-9)
            pos = 0.0;
    }

    internalBeat = startBeat + blockBeats;
}

void DrumWoodysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    const juce::SpinLock::ScopedTryLockType lock (audioLock);
    if (! lock.isLocked()) // kit or groove being swapped right now
    {
        buffer.clear();
        return;
    }

    if (resetVoices)
    {
        engine.reset();
        resetVoices = false;
    }

    renderSequence (midi, buffer.getNumSamples());
    engine.process (buffer, midi, audioKit, readMixSettings());
}

//==============================================================================
void DrumWoodysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DRUMWOODYS");
    root.appendChild (parameters.copyState(), nullptr);

    juce::ValueTree kitTree ("KIT");
    for (int s = 0; s < numSlots; ++s)
        kitTree.setProperty (slotDefs()[(size_t) s].id, selection[(size_t) s], nullptr);
    root.appendChild (kitTree, nullptr);

    juce::ValueTree songTree ("SONG");
    for (const auto& part : song)
    {
        auto partTree = part.pattern.toValueTree();
        partTree.setProperty ("repeats", part.repeats, nullptr);
        songTree.appendChild (partTree, nullptr);
    }
    root.appendChild (songTree, nullptr);

    juce::ValueTree editTree ("EDIT");
    editTree.appendChild (editPattern.toValueTree(), nullptr);
    root.appendChild (editTree, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumWoodysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("DRUMWOODYS"))
        return;

    const auto root = juce::ValueTree::fromXml (*xml);

    const auto paramsTree = root.getChildWithName (parameters.state.getType());
    if (paramsTree.isValid())
        parameters.replaceState (paramsTree);

    const auto kitTree = root.getChildWithName ("KIT");
    for (int s = 0; s < numSlots; ++s)
    {
        const auto id = kitTree.getProperty (slotDefs()[(size_t) s].id).toString();
        selection[(size_t) s] = library->find (id) != nullptr ? id : library->defaultFor (s);
    }

    song.clear();
    for (const auto& partTree : root.getChildWithName ("SONG"))
        song.push_back ({ Pattern::fromValueTree (partTree), juce::jmax (1, (int) partTree.getProperty ("repeats", 1)) });

    const auto editTree = root.getChildWithName ("EDIT").getChild (0);
    if (editTree.isValid())
        editPattern = Pattern::fromValueTree (editTree);

    rebuildKit();
}

juce::AudioProcessorEditor* DrumWoodysProcessor::createEditor()
{
    return new DrumWoodysEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumWoodysProcessor();
}
