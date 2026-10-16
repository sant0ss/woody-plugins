#include "DrumEngine.h"
#include <cmath>

namespace woody
{
namespace
{
// Equal-gain balance: centre leaves both sides untouched.
void balance (float pan, float& left, float& right)
{
    left  = juce::jmin (1.0f, 1.0f - pan);
    right = juce::jmin (1.0f, 1.0f + pan);
}
} // namespace

void DrumEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& b : slotBuffers)
        b.setSize (2, chunkSize);

    roomBuffer.setSize (2, chunkSize);
    reverb.setSampleRate (sampleRate);
    reset();
}

void DrumEngine::reset()
{
    for (auto& v : voices)
        v = {};

    for (auto& rr : roundRobin)
        rr.fill (0);

    reverb.reset();
}

void DrumEngine::process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi, const Kit* kit, const MixSettings& mix)
{
    output.clear();

    juce::Reverb::Parameters rp;
    rp.roomSize = mix.roomSize;
    rp.damping = 0.45f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters (rp);

    const int total = output.getNumSamples();
    auto event = midi.begin();

    for (int chunkStart = 0; chunkStart < total; chunkStart += chunkSize)
    {
        const int chunkLength = juce::jmin (chunkSize, total - chunkStart);
        const int chunkEnd = chunkStart + chunkLength;

        for (auto& b : slotBuffers)
            b.clear (0, chunkLength);

        int rendered = 0;

        for (; event != midi.end() && (*event).samplePosition < chunkEnd; ++event)
        {
            const auto metadata = *event;
            const int offset = juce::jlimit (0, chunkLength, metadata.samplePosition - chunkStart);

            renderVoices (rendered, offset - rendered);
            rendered = offset;

            const auto message = metadata.getMessage();
            if (kit != nullptr && message.isNoteOn())
                noteOn (*kit, message.getNoteNumber(), message.getVelocity(), mix);
            else if (message.isAllNotesOff() || message.isAllSoundOff())
                for (auto& v : voices)
                    v.active = false;
        }

        renderVoices (rendered, chunkLength - rendered);
        mixChunk (output, chunkStart, chunkLength, mix);
    }
}

DrumEngine::Voice& DrumEngine::allocateVoice()
{
    Voice* oldest = &voices[0];

    for (auto& v : voices)
    {
        if (! v.active)
            return v;

        if (v.age < oldest->age)
            oldest = &v;
    }

    return *oldest;
}

void DrumEngine::chokeSlot (int slot)
{
    const float fadeStep = 1.0f / (0.006f * (float) sampleRate); // 6 ms

    for (auto& v : voices)
        if (v.active && v.slot == slot && v.fadeStep == 0.0f)
            v.fadeStep = fadeStep;
}

void DrumEngine::noteOn (const Kit& kit, int note, int velocity, const MixSettings& mix)
{
    const auto target = noteMap()[(size_t) note];
    if (target.slot < 0)
        return;

    const auto& def = slotDefs()[(size_t) target.slot];
    const auto& articulation = def.articulations[(size_t) target.articulation];

    // A new hi-hat stroke cuts the previous one; a choke stops the cymbal ringing
    if (def.type == hihatType || articulation.choke)
        chokeSlot (target.slot);

    const auto& slot = kit.slots[(size_t) target.slot];
    const auto* layers = &slot.articulations[(size_t) target.articulation];

    // Missing articulation: fall back to the main hit (a missing choke stays silent)
    if (layers->empty() && ! articulation.choke)
        layers = &slot.articulations[0];

    hitCounters[(size_t) target.slot].fetch_add (1, std::memory_order_relaxed);

    if (layers->empty())
        return;

    const int rr = roundRobin[(size_t) target.slot][(size_t) target.articulation]++;
    const float randomValue = random.nextFloat();
    const float humanize = mix.humanize;
    const float tuneCents = mix.strips[(size_t) target.slot].tune * 100.0f;

    // Velocity scales gain inside each layer; the layers carry the timbre.
    const float velocityGain = std::pow ((float) velocity / 127.0f, 1.6f);

    for (const auto& layer : *layers)
    {
        if (velocity < layer.loVel || velocity > layer.hiVel
            || randomValue < layer.loRand || randomValue >= layer.hiRand
            || (layer.seqLength > 1 && (rr % layer.seqLength) + 1 != layer.seqPosition))
            continue;

        const float humanGainDb = (random.nextFloat() * 2.0f - 1.0f) * 3.0f * humanize;
        const float humanCents = (random.nextFloat() * 2.0f - 1.0f) * 12.0f * humanize;

        auto& v = allocateVoice();
        v.sample = layer.sample.get();
        v.slot = target.slot;
        v.position = 0.0;
        v.increment = layer.sample->sampleRate / sampleRate
                      * std::pow (2.0, (layer.tuneCents + tuneCents + humanCents) / 1200.0);
        v.gain = velocityGain * layer.gain * juce::Decibels::decibelsToGain (humanGainDb);
        v.fade = 1.0f;
        v.fadeStep = 0.0f;
        v.age = ++voiceAge;
        v.active = true;
    }
}

void DrumEngine::renderVoices (int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& src = v.sample->buffer;
        const int length = src.getNumSamples();
        const float* inL = src.getReadPointer (0);
        const float* inR = src.getReadPointer (src.getNumChannels() > 1 ? 1 : 0);

        auto& dest = slotBuffers[(size_t) v.slot];
        float* outL = dest.getWritePointer (0, startSample);
        float* outR = dest.getWritePointer (1, startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            const int index = (int) v.position;
            if (index + 1 >= length || v.fade <= 0.0f)
            {
                v.active = false;
                break;
            }

            const float frac = (float) (v.position - index);
            const float g = v.gain * v.fade;

            outL[i] += (inL[index] + frac * (inL[index + 1] - inL[index])) * g;
            outR[i] += (inR[index] + frac * (inR[index + 1] - inR[index])) * g;

            v.position += v.increment;
            v.fade -= v.fadeStep;
        }
    }
}

void DrumEngine::mixChunk (juce::AudioBuffer<float>& output, int startSample, int numSamples, const MixSettings& mix)
{
    roomBuffer.clear (0, numSamples);

    const bool stereo = output.getNumChannels() > 1;

    bool anySolo = false;
    for (const auto& s : mix.strips)
        anySolo = anySolo || s.solo;

    for (int c = 0; c < numSlots; ++c)
    {
        const auto& strip = mix.strips[(size_t) c];
        const bool audible = ! strip.mute && (! anySolo || strip.solo);
        const float level = audible ? juce::Decibels::decibelsToGain (strip.levelDb, -60.0f) : 0.0f;

        float panL, panR;
        balance (strip.pan, panL, panR);

        const float targetL = level * panL;
        const float targetR = level * panR;
        const float targetSend = level * strip.room;
        const auto& src = slotBuffers[(size_t) c];

        output.addFromWithRamp (0, startSample, src.getReadPointer (0), numSamples, lastL[(size_t) c], targetL);
        if (stereo)
            output.addFromWithRamp (1, startSample, src.getReadPointer (1), numSamples, lastR[(size_t) c], targetR);

        roomBuffer.addFromWithRamp (0, 0, src.getReadPointer (0), numSamples, lastSend[(size_t) c], targetSend);
        roomBuffer.addFromWithRamp (1, 0, src.getReadPointer (1), numSamples, lastSend[(size_t) c], targetSend);

        lastL[(size_t) c] = targetL;
        lastR[(size_t) c] = targetR;
        lastSend[(size_t) c] = targetSend;
    }

    reverb.processStereo (roomBuffer.getWritePointer (0), roomBuffer.getWritePointer (1), numSamples);

    output.addFromWithRamp (0, startSample, roomBuffer.getReadPointer (0), numSamples, lastAmbience, mix.ambience);
    if (stereo)
        output.addFromWithRamp (1, startSample, roomBuffer.getReadPointer (1), numSamples, lastAmbience, mix.ambience);
    lastAmbience = mix.ambience;

    const float master = juce::Decibels::decibelsToGain (mix.masterDb, -60.0f);
    for (int ch = 0; ch < output.getNumChannels(); ++ch)
        output.applyGainRamp (ch, startSample, numSamples, lastMaster, master);
    lastMaster = master;
}
} // namespace woody
