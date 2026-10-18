#include "PluginProcessor.h"
#include "PluginEditor.h"

SatProcessor::SatProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "SatWoodys", satwoody::createLayout())
{
    auto raw = [this] (const char* id) { return parameters.getRawParameterValue (id); };

    inParam = raw ("in");
    driveParam = raw ("drive");
    typeParam = raw ("type");
    toneParam = raw ("tone");
    biasParam = raw ("bias");
    mixParam = raw ("mix");
    outParam = raw ("out");
    autoParam = raw ("auto");
    stereoParam = raw ("stereo");
    focusLowParam = raw ("focuslow");
    focusHighParam = raw ("focushigh");
    osParam = raw ("os");
    bypassParam = raw ("bypass");

    parameters.addParameterListener ("os", this);

    startTimerHz (30);
}

SatProcessor::~SatProcessor()
{
    stopTimer();
}

bool SatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    latencyDirty = true;
    timerCallback();
}

// Message thread: reports the latency of the chosen oversampling to the host
void SatProcessor::timerCallback()
{
    if (! latencyDirty.exchange (false))
        return;

    const int wanted = engine.latencyFor ((int) osParam->load());
    if (wanted != latency.load())
    {
        latency = wanted;
        setLatencySamples (wanted);
    }
}

float SatProcessor::getValue (const juce::String& id) const
{
    if (auto* v = parameters.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

satwoody::Settings SatProcessor::readSettings() const
{
    satwoody::Settings s;
    s.inputDb = inParam->load();
    s.drive = driveParam->load();
    s.type = (int) typeParam->load();
    s.tone = toneParam->load();
    s.bias = biasParam->load();
    s.mix = mixParam->load();
    s.outputDb = outParam->load();
    s.autoGain = autoParam->load() > 0.5f;
    s.stereo = (int) stereoParam->load();
    s.focusLow = focusLowParam->load();
    s.focusHigh = focusHighParam->load();
    s.oversampling = (int) osParam->load();
    s.bypass = bypassParam->load() > 0.5f;
    return s;
}

void SatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), readSettings());
}

//==============================================================================
void SatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("editorWidth", editorWidth, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    editorWidth = (int) state.getProperty ("editorWidth", 0);
    parameters.replaceState (state);
    latencyDirty = true;
}

juce::AudioProcessorEditor* SatProcessor::createEditor()
{
    return new SatEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SatProcessor();
}
