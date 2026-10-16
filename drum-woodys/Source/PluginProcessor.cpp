#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace woody;

namespace
{
constexpr auto libraryFolderKey = "libraryFolder";

// Per-player settings that a kit preset leaves alone
const juce::StringArray nonKitParameters { "swing", "followHost", "timing", "master" };
} // namespace

juce::String DrumWoodysProcessor::paramId (int slot, const char* name)
{
    return juce::String (slotDefs()[(size_t) slot].id) + "_" + name;
}

juce::AudioProcessor::BusesProperties DrumWoodysProcessor::createBuses()
{
    // Main mix plus one optional stereo output per kit piece, for multi-out in the DAW
    auto buses = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (const auto& def : slotDefs())
        buses = buses.withOutput (def.name, juce::AudioChannelSet::stereo(), false);
    return buses;
}

DrumWoodysProcessor::DrumWoodysProcessor()
    : AudioProcessor (createBuses()),
      parameters (*this, nullptr, "DrumWoodys", createLayout())
{
    for (int s = 0; s < numSlots; ++s)
    {
        auto& p = stripParams[(size_t) s];
        auto get = [&] (const char* name) { return parameters.getRawParameterValue (paramId (s, name)); };
        p.level = get ("level");
        p.pan = get ("pan");
        p.room = get ("room");
        p.overhead = get ("oh");
        p.tune = get ("tune");
        p.mute = get ("mute");
        p.solo = get ("solo");
        p.out = get ("out");
        p.eqLow = get ("eqlow");
        p.eqMid = get ("eqmid");
        p.eqFreq = get ("eqfreq");
        p.eqHigh = get ("eqhigh");
        p.comp = get ("comp");
        p.attack = get ("attack");
        p.sustain = get ("sustain");
        p.drive = get ("drive");
    }

    masterParam       = parameters.getRawParameterValue ("master");
    overheadParam     = parameters.getRawParameterValue ("ohLevel");
    overheadMuteParam = parameters.getRawParameterValue ("ohMute");
    roomParam         = parameters.getRawParameterValue ("roomLevel");
    roomMuteParam     = parameters.getRawParameterValue ("roomMute");
    roomSizeParam     = parameters.getRawParameterValue ("roomSize");
    roomCompParam     = parameters.getRawParameterValue ("roomComp");
    glueParam         = parameters.getRawParameterValue ("glue");
    humanizeParam     = parameters.getRawParameterValue ("humanize");
    variationParam    = parameters.getRawParameterValue ("variation");
    timingParam       = parameters.getRawParameterValue ("timing");
    followHostParam   = parameters.getRawParameterValue ("followHost");

    editPattern.name = "Meu groove";
    editPattern.genre = juce::String::fromUTF8 ("Meus padrões");

    juce::File folder;
    if (auto settings = openSettings())
        folder = juce::File (settings->getValue (libraryFolderKey));

    library = Library::scan (folder);
    applyFactoryPreset (factoryPresets().front());
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumWoodysProcessor::createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Bool = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    Range dbRange (-60.0f, 6.0f, 0.1f);
    dbRange.setSkewForCentre (-12.0f);
    const Range unit (0.0f, 1.0f, 0.01f);
    const Range bipolar (-1.0f, 1.0f, 0.01f);
    const Range eqGain (-12.0f, 12.0f, 0.1f);
    Range eqFreq (200.0f, 8000.0f, 1.0f);
    eqFreq.setSkewForCentre (1200.0f);
    const auto dB = juce::AudioParameterFloatAttributes().withLabel ("dB");

    auto add = [&] (std::unique_ptr<juce::RangedAudioParameter> p) { layout.add (std::move (p)); };

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& def = slotDefs()[(size_t) s];
        const juce::String n (def.name);
        auto id = [&] (const char* name) { return juce::ParameterID { paramId (s, name), 1 }; };

        add (std::make_unique<Float> (id ("level"), n + " Level", dbRange, 0.0f, dB));
        add (std::make_unique<Float> (id ("pan"), n + " Pan", bipolar, def.defaultPan));
        add (std::make_unique<Float> (id ("room"), n + " Room Send", unit, def.defaultRoom));
        add (std::make_unique<Float> (id ("oh"), n + " Overhead Send", unit, def.defaultOverhead));
        add (std::make_unique<Float> (id ("tune"), n + " Tune", Range (-12.0f, 12.0f, 0.1f), 0.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("st")));
        add (std::make_unique<Bool> (id ("mute"), n + " Mute", false));
        add (std::make_unique<Bool> (id ("solo"), n + " Solo", false));
        add (std::make_unique<juce::AudioParameterChoice> (id ("out"), n + " Output", juce::StringArray { "Main", "Separate" }, 0));
        add (std::make_unique<Float> (id ("eqlow"), n + " EQ Low", eqGain, 0.0f, dB));
        add (std::make_unique<Float> (id ("eqmid"), n + " EQ Mid", eqGain, 0.0f, dB));
        add (std::make_unique<Float> (id ("eqfreq"), n + " EQ Mid Freq", eqFreq, 1000.0f,
                                      juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        add (std::make_unique<Float> (id ("eqhigh"), n + " EQ High", eqGain, 0.0f, dB));
        add (std::make_unique<Float> (id ("comp"), n + " Compressor", unit, 0.0f));
        add (std::make_unique<Float> (id ("attack"), n + " Attack", bipolar, 0.0f));
        add (std::make_unique<Float> (id ("sustain"), n + " Sustain", bipolar, 0.0f));
        add (std::make_unique<Float> (id ("drive"), n + " Drive", unit, 0.0f));
    }

    add (std::make_unique<Float> (juce::ParameterID { "master", 1 }, "Master", dbRange, 0.0f, dB));
    add (std::make_unique<Float> (juce::ParameterID { "ohLevel", 1 }, "Overhead Level", dbRange, -6.0f, dB));
    add (std::make_unique<Bool> (juce::ParameterID { "ohMute", 1 }, "Overhead Mute", false));
    add (std::make_unique<Float> (juce::ParameterID { "roomLevel", 1 }, "Room Level", dbRange, -8.0f, dB));
    add (std::make_unique<Bool> (juce::ParameterID { "roomMute", 1 }, "Room Mute", false));
    add (std::make_unique<Float> (juce::ParameterID { "roomSize", 1 }, "Room Size", unit, 0.4f));
    add (std::make_unique<Float> (juce::ParameterID { "roomComp", 1 }, "Room Compression", unit, 0.3f));
    add (std::make_unique<Float> (juce::ParameterID { "glue", 1 }, "Master Glue", unit, 0.2f));
    add (std::make_unique<Float> (juce::ParameterID { "humanize", 1 }, "Humanize", unit, 0.3f));
    add (std::make_unique<Float> (juce::ParameterID { "variation", 1 }, "Variation", unit, 0.5f));
    add (std::make_unique<Float> (juce::ParameterID { "timing", 1 }, "Groove Timing", unit, 0.2f));
    add (std::make_unique<Float> (juce::ParameterID { "swing", 1 }, "Swing", unit, 0.0f));
    add (std::make_unique<Bool> (juce::ParameterID { "followHost", 1 }, "Follow Host", false));
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

//==============================================================================
void DrumWoodysProcessor::setParameter (const juce::String& id, float value)
{
    if (auto* p = parameters.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

bool DrumWoodysProcessor::isKitParameter (const juce::String& id) const
{
    return ! nonKitParameters.contains (id);
}

void DrumWoodysProcessor::applyFactoryPreset (const FactoryPreset& preset)
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && isKitParameter (ranged->getParameterID()))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : preset.params)
        setParameter (id, value);

    for (int s = 0; s < numSlots; ++s)
    {
        juce::String kitName (preset.mainKit);
        for (const auto& [slot, borrowed] : preset.slotKits)
            if (slot == s)
                kitName = borrowed;

        selection[(size_t) s] = library->defaultFor (s, kitName);
    }

    currentPresetName = preset.name;
    rebuildKit();
}

juce::ValueTree DrumWoodysProcessor::capturePreset (const juce::String& name) const
{
    juce::ValueTree preset ("PRESET");
    preset.setProperty ("name", name, nullptr);

    juce::ValueTree kitTree ("KIT");
    for (int s = 0; s < numSlots; ++s)
        kitTree.setProperty (slotDefs()[(size_t) s].id, selection[(size_t) s], nullptr);
    preset.appendChild (kitTree, nullptr);

    juce::ValueTree params ("PARAMS");
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && isKitParameter (ranged->getParameterID()))
            params.setProperty (ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()), nullptr);
    preset.appendChild (params, nullptr);

    return preset;
}

void DrumWoodysProcessor::applyUserPreset (const juce::ValueTree& preset)
{
    const auto params = preset.getChildWithName ("PARAMS");
    for (int i = 0; i < params.getNumProperties(); ++i)
    {
        const auto id = params.getPropertyName (i).toString();
        if (isKitParameter (id))
            setParameter (id, (float) params.getProperty (id));
    }

    const auto kitTree = preset.getChildWithName ("KIT");
    for (int s = 0; s < numSlots; ++s)
    {
        const auto id = kitTree.getProperty (slotDefs()[(size_t) s].id).toString();
        selection[(size_t) s] = library->find (id) != nullptr ? id : library->defaultFor (s);
    }

    currentPresetName = preset.getProperty ("name").toString();
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
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono())
        return false;

    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& set = layouts.outputBuses.getReference (i);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }

    return layouts.inputBuses.isEmpty();
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
        strip.overhead = p.overhead->load();
        strip.tune = p.tune->load();
        strip.mute = p.mute->load() > 0.5f;
        strip.solo = p.solo->load() > 0.5f;
        strip.separateOut = p.out->load() > 0.5f;
        strip.eqLow = p.eqLow->load();
        strip.eqMid = p.eqMid->load();
        strip.eqMidFreq = p.eqFreq->load();
        strip.eqHigh = p.eqHigh->load();
        strip.comp = p.comp->load();
        strip.attack = p.attack->load();
        strip.sustain = p.sustain->load();
        strip.drive = p.drive->load();
    }

    mix.masterDb = masterParam->load();
    mix.overheadDb = overheadParam->load();
    mix.overheadMute = overheadMuteParam->load() > 0.5f;
    mix.roomDb = roomParam->load();
    mix.roomMute = roomMuteParam->load() > 0.5f;
    mix.roomSize = roomSizeParam->load();
    mix.roomComp = roomCompParam->load();
    mix.glue = glueParam->load();
    mix.humanize = humanizeParam->load();
    mix.variation = variationParam->load();
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
    const double maxJitter = timingParam->load() * 0.012 * currentSampleRate; // up to 12 ms

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
            const double jitter = maxJitter > 0.0 ? (timingRandom.nextDouble() * 2.0 - 1.0) * maxJitter : 0.0;
            const int offset = juce::jlimit (0, numSamples - 1, (int) ((offsetBeats + it->beat - pos) / beatsPerSample + jitter));
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
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    const juce::SpinLock::ScopedTryLockType lock (audioLock);
    if (! lock.isLocked()) // kit or groove being swapped right now
        return;

    if (resetVoices)
    {
        engine.reset();
        resetVoices = false;
    }

    auto main = getBusBuffer (buffer, false, 0);

    std::array<juce::AudioBuffer<float>*, numSlots> aux {};
    for (int s = 0; s < numSlots; ++s)
    {
        const int busIndex = s + 1;
        if (busIndex < getBusCount (false) && getBus (false, busIndex)->isEnabled())
        {
            auxViews[(size_t) s] = getBusBuffer (buffer, false, busIndex);
            if (auxViews[(size_t) s].getNumChannels() > 0)
                aux[(size_t) s] = &auxViews[(size_t) s];
        }
    }

    renderSequence (midi, buffer.getNumSamples());
    engine.process (main, aux, midi, audioKit, readMixSettings());
}

//==============================================================================
void DrumWoodysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("DRUMWOODYS");
    root.setProperty ("preset", currentPresetName, nullptr);
    root.setProperty ("editorWidth", editorWidth, nullptr);
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
    currentPresetName = root.getProperty ("preset").toString();
    editorWidth = (int) root.getProperty ("editorWidth", 0);

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
