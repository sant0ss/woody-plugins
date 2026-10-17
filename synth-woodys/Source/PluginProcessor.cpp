#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace synthwoody;

SynthWoodysProcessor::SynthWoodysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "SynthWoodys", createLayout())
{
    for (int i = 0; i < numParams; ++i)
        raw[(size_t) i] = parameters.getRawParameterValue (paramDefs()[(size_t) i].id);

    // Building the wavetables takes a moment, so do it here and not on the audio thread
    WavetableBank::get();

    currentPresetName = factoryPresets().front().name;
}

void SynthWoodysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    effects.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

bool SynthWoodysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.inputBuses.isEmpty();
}

Settings SynthWoodysProcessor::readSettings() const
{
    Settings s;

    s.aShape = (int) get (aShape);
    s.aPw = get (aPw);
    s.aSemi = get (aSemi);
    s.aFine = get (aFine);
    s.aLevel = get (aLevel);

    s.bTable = (int) get (bTable);
    s.bPos = get (bPos);
    s.bSemi = get (bSemi);
    s.bFine = get (bFine);
    s.bLevel = get (bLevel);

    s.unison = (int) get (unison);
    s.uniDetune = get (uniDetune);
    s.uniSpread = get (uniSpread);
    s.subLevel = get (subLevel);
    s.noiseLevel = get (noiseLevel);

    s.fType = (int) get (fType);
    s.cutoff = get (cutoff);
    s.res = get (res);
    s.drive = get (fDrive);
    s.envAmt = get (envAmt);
    s.keytrack = get (keytrack);

    s.amp = { get (ampA), get (ampD), get (ampS), get (ampR) };
    s.flt = { get (fltA), get (fltD), get (fltS), get (fltR) };

    s.lfo[0] = { (int) get (lfo1Shape), get (lfo1Rate) };
    s.lfo[1] = { (int) get (lfo2Shape), get (lfo2Rate) };

    for (int i = 0; i < numModSlots; ++i)
    {
        const int first = (int) m1Src + 3 * i;
        s.mod[(size_t) i] = { (int) get ((ParamIndex) first), (int) get ((ParamIndex) (first + 1)), get ((ParamIndex) (first + 2)) };
    }

    return s;
}

FxSettings SynthWoodysProcessor::readFxSettings() const
{
    FxSettings fx;
    fx.drive = get (fxDrive);
    fx.chorusMix = get (chMix);
    fx.chorusRate = get (chRate);
    fx.chorusDepth = get (chDepth);
    fx.delayMix = get (dlMix);
    fx.delayMs = get (dlTime);
    fx.delayFeedback = get (dlFb);
    fx.reverbMix = get (rvMix);
    fx.reverbSize = get (rvSize);
    fx.masterDb = get (master);
    return fx;
}

void SynthWoodysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    if (panicRequested.exchange (false))
        synth.reset();

    synth.process (buffer, midi, readSettings());
    effects.process (buffer, readFxSettings());

    outputPeak = juce::jmax (buffer.getMagnitude (0, buffer.getNumSamples()), outputPeak.load() * 0.9f);
}

//==============================================================================
void SynthWoodysProcessor::setParameter (const juce::String& id, float value)
{
    if (auto* p = parameters.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void SynthWoodysProcessor::applyFactoryPreset (const FactoryPreset& preset)
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        setParameter (id, (float) value);

    currentPresetName = preset.name;
}

juce::ValueTree SynthWoodysProcessor::capturePreset (const juce::String& name) const
{
    juce::ValueTree preset ("PRESET");
    preset.setProperty ("name", name, nullptr);

    juce::ValueTree params ("PARAMS");
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            params.setProperty (ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()), nullptr);

    preset.appendChild (params, nullptr);
    return preset;
}

void SynthWoodysProcessor::applyUserPreset (const juce::ValueTree& preset)
{
    const auto params = preset.getChildWithName ("PARAMS");
    for (int i = 0; i < params.getNumProperties(); ++i)
    {
        const auto id = params.getPropertyName (i).toString();
        setParameter (id, (float) params.getProperty (id));
    }

    currentPresetName = preset.getProperty ("name").toString();
}

static juce::File userPresetsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Woody Plugins")
        .getChildFile ("Synth Woodys Presets.xml");
}

std::vector<juce::ValueTree> SynthWoodysProcessor::loadUserPresets()
{
    std::vector<juce::ValueTree> presets;
    if (auto xml = juce::XmlDocument::parse (userPresetsFile()))
        for (const auto& child : juce::ValueTree::fromXml (*xml))
            presets.push_back (child.createCopy());
    return presets;
}

void SynthWoodysProcessor::saveUserPresets (const std::vector<juce::ValueTree>& presets)
{
    juce::ValueTree tree ("USER_PRESETS");
    for (const auto& p : presets)
        tree.appendChild (p.createCopy(), nullptr);

    auto file = userPresetsFile();
    file.getParentDirectory().createDirectory();
    if (auto xml = tree.createXml())
        xml->writeTo (file);
}

//==============================================================================
void SynthWoodysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("SYNTHWOODYS");
    root.setProperty ("preset", currentPresetName, nullptr);
    root.setProperty ("editorWidth", editorWidth, nullptr);
    root.appendChild (parameters.copyState(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthWoodysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("SYNTHWOODYS"))
        return;

    const auto root = juce::ValueTree::fromXml (*xml);
    currentPresetName = root.getProperty ("preset").toString();
    editorWidth = (int) root.getProperty ("editorWidth", 0);

    const auto paramsTree = root.getChildWithName (parameters.state.getType());
    if (paramsTree.isValid())
        parameters.replaceState (paramsTree);
}

juce::AudioProcessorEditor* SynthWoodysProcessor::createEditor()
{
    return new SynthWoodysEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthWoodysProcessor();
}
