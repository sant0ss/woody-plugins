#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace eqwoody;

namespace
{
constexpr int maxLatency = 8192; // half of the longest impulse response
}

EqProcessor::EqProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "EqWoodys", createLayout())
{
    static const char* names[9] = { "on", "type", "freq", "gain", "q", "slope", "place", "sat", "satmode" };

    for (int b = 0; b < numBands; ++b)
        for (int p = 0; p < 9; ++p)
        {
            const auto id = bandId (b, names[p]);
            bandParams[(size_t) b][(size_t) p] = parameters.getRawParameterValue (id);
            parameters.addParameterListener (id, this);
        }

    outParam = parameters.getRawParameterValue ("out");
    phaseParam = parameters.getRawParameterValue ("phase");
    qualityParam = parameters.getRawParameterValue ("quality");
    bypassParam = parameters.getRawParameterValue ("bypass");

    parameters.addParameterListener ("phase", this);
    parameters.addParameterListener ("quality", this);

    startTimerHz (30);
}

EqProcessor::~EqProcessor()
{
    stopTimer();
}

bool EqProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EqProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    maxBlock = juce::jmax (64, samplesPerBlock);

    engine.prepare (sampleRate, maxBlock);
    linearFilter.prepare (sampleRate, maxBlock);

    for (auto& d : satDelay)
        d.init (maxLatency * 2 + 8);

    satLeft.assign ((size_t) maxBlock, 0.0f);
    satRight.assign ((size_t) maxBlock, 0.0f);

    filterDirty = true;
    timerCallback(); // build the first impulse response right away
}

BandsArray EqProcessor::readBands() const
{
    BandsArray bands;

    for (int b = 0; b < numBands; ++b)
    {
        const auto& p = bandParams[(size_t) b];
        auto& s = bands[(size_t) b];

        s.on = p[0]->load() > 0.5f;
        s.type = (int) p[1]->load();
        s.freq = p[2]->load();
        s.gain = p[3]->load();
        s.q = p[4]->load();
        s.slope = (int) p[5]->load();
        s.place = (int) p[6]->load();
        s.sat = p[7]->load();
        s.satMode = (int) p[8]->load();
    }

    return bands;
}

float EqProcessor::getValue (const juce::String& id) const
{
    if (auto* v = parameters.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

void EqProcessor::setValue (const juce::String& id, float realValue)
{
    if (auto* p = parameters.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void EqProcessor::resetAll()
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->getParameterID() != "analyzer" && ranged->getParameterID() != "range")
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
}

// Message thread: rebuilds the linear-phase filter when something changed, and reports the latency.
void EqProcessor::timerCallback()
{
    if (! filterDirty.exchange (false) || sampleRate <= 0.0)
        return;

    const bool linear = phaseParam->load() > 0.5f;
    const int length = impulseLength ((int) qualityParam->load());
    const int wantedLatency = linear ? length / 2 : 0;

    if (wantedLatency != latency.load())
    {
        latency = wantedLatency;
        setLatencySamples (wantedLatency);
    }

    if (linear)
    {
        const auto impulses = buildImpulses (readBands(), sampleRate, length);
        needsCross = impulses.needsCross;
        linearFilter.load (impulses, sampleRate);
    }
}

void EqProcessor::processChunk (float* left, float* right, int numSamples)
{
    const auto bands = readBands();
    const bool linear = phaseParam->load() > 0.5f;

    bool anySaturation = false;
    for (const auto& b : bands)
        anySaturation = anySaturation || (b.on && b.sat >= 0.001f);

    // Saturation reads the dry input, so compute it before the EQ touches the buffers
    if (anySaturation)
        engine.processSaturation (left, right, numSamples, bands, satLeft.data(), satRight.data());
    else
    {
        std::fill (satLeft.begin(), satLeft.begin() + numSamples, 0.0f);
        std::fill (satRight.begin(), satRight.begin() + numSamples, 0.0f);
    }

    if (linear)
    {
        linearFilter.process (left, right, numSamples, needsCross.load());

        // The harmonics must wait as long as the filtered signal does
        const int delay = juce::jlimit (0, maxLatency * 2, latency.load());
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] += satDelay[0].process (satLeft[(size_t) i], delay);
            right[i] += satDelay[1].process (satRight[(size_t) i], delay);
        }
    }
    else
    {
        engine.processMinPhase (left, right, numSamples, bands);

        for (int i = 0; i < numSamples; ++i)
        {
            left[i] += satLeft[(size_t) i];
            right[i] += satRight[(size_t) i];
        }
    }
}

void EqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = buffer.getNumSamples();
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    preFifo.push (left, right, total);

    if (bypassParam->load() < 0.5f)
    {
        for (int start = 0; start < total; start += maxBlock)
            processChunk (left + start, right + start, juce::jmin (maxBlock, total - start));

        const float gain = juce::Decibels::decibelsToGain (outParam->load(), -60.0f);
        buffer.applyGainRamp (0, total, lastOut, gain);
        lastOut = gain;
    }

    postFifo.push (left, right, total);
    outputPeak = juce::jmax (buffer.getMagnitude (0, total), outputPeak.load() * 0.9f);
}

//==============================================================================
void EqProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("editorWidth", editorWidth, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EqProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    editorWidth = (int) state.getProperty ("editorWidth", 0);
    parameters.replaceState (state);
    filterDirty = true;
}

juce::AudioProcessorEditor* EqProcessor::createEditor()
{
    return new EqEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqProcessor();
}
