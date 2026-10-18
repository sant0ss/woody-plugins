#include "PluginProcessor.h"
#include "PluginEditor.h"

CompProcessor::CompProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "CompWoodys", compwoody::createLayout()),
      paramSet (parameters)
{
    startTimerHz (20);
}

CompProcessor::~CompProcessor()
{
    stopTimer();
}

bool CompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CompProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    engine.prepare (sampleRate, samplesPerBlock);

    latency = compwoody::CompEngine::latencyFor (paramSet.read(), sampleRate);
    setLatencySamples (latency);
}

float CompProcessor::getValue (const juce::String& id) const
{
    if (auto* v = parameters.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

void CompProcessor::setValue (const juce::String& id, float realValue)
{
    if (auto* p = parameters.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

// Message thread: reports the latency of the lookahead setting to the host
void CompProcessor::timerCallback()
{
    if (sampleRate <= 0.0)
        return;

    const int wanted = compwoody::CompEngine::latencyFor (paramSet.read(), sampleRate);
    if (wanted != latency)
    {
        latency = wanted;
        setLatencySamples (wanted);
    }
}

void CompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = buffer.getNumSamples();
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    inputPeak = juce::jmax (inputPeak.load(), buffer.getMagnitude (0, total));

    engine.process (left, right, total, paramSet.read());

    outputPeak = juce::jmax (outputPeak.load(), buffer.getMagnitude (0, total));
}

//==============================================================================
void CompProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("editorWidth", editorWidth, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    editorWidth = (int) state.getProperty ("editorWidth", 0);
    parameters.replaceState (state);
}

juce::AudioProcessorEditor* CompProcessor::createEditor()
{
    return new CompEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompProcessor();
}
