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

// Small differences applied to repeated hits so the same sample never sounds twice the same.
struct Variant
{
    int layerShift;   // use the neighbouring velocity layer
    float cents;
    float gainDb;
    float offsetMs;   // start a little into the sample
};

constexpr Variant variants[] = {
    {  0,  0.0f,  0.0f, 0.00f },
    { -1, -6.0f,  1.8f, 0.00f },
    {  0,  7.0f, -0.8f, 0.35f },
    {  1,  3.0f, -2.0f, 0.15f },
};
} // namespace

void DrumEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& b : slotBuffers)
        b.setSize (2, chunkSize);

    roomBuffer.setSize (2, chunkSize);
    overheadBuffer.setSize (2, chunkSize);
    mainChunk.setSize (2, chunkSize);

    reverb.setSampleRate (sampleRate);

    for (auto& fx : slotFx)
    {
        fx.comp.prepare (sampleRate);
        fx.transient.prepare (sampleRate);
        fx.lastLow = fx.lastMid = fx.lastHigh = 1.0e9f;
        fx.lastComp = -1.0f;
    }

    roomCompressor.prepare (sampleRate);
    glueCompressor.prepare (sampleRate);
    lastRoomComp = lastGlue = -1.0f;

    overheadLowCut.setHighPass (sampleRate, 380.0);
    overheadAir.setHighShelf (sampleRate, 9000.0, 2.5);

    reset();
}

void DrumEngine::reset()
{
    for (auto& v : voices)
        v = {};

    for (auto& rr : roundRobin)
        rr.fill (0);

    for (auto& fx : slotFx)
    {
        fx.low.reset();
        fx.mid.reset();
        fx.high.reset();
        fx.comp.reset();
        fx.transient.reset();
    }

    overheadLowCut.reset();
    overheadAir.reset();
    roomCompressor.reset();
    glueCompressor.reset();
    reverb.reset();
}

void DrumEngine::process (juce::AudioBuffer<float>& main, const std::array<juce::AudioBuffer<float>*, numSlots>& aux,
                          const juce::MidiBuffer& midi, const Kit* kit, const MixSettings& mix)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = mix.roomSize;
    rp.damping = 0.45f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters (rp);

    if (mix.roomComp != lastRoomComp)
    {
        lastRoomComp = mix.roomComp;
        roomCompressor.setParameters (-10.0f - mix.roomComp * 26.0f, 4.0f, 2.0f, 120.0f, mix.roomComp * 12.0f);
    }

    if (mix.glue != lastGlue)
    {
        lastGlue = mix.glue;
        const float ratio = 2.0f + mix.glue * 2.0f;
        const float threshold = -mix.glue * 20.0f;
        glueCompressor.setParameters (threshold, ratio, 10.0f, 150.0f, -threshold * (1.0f - 1.0f / ratio) * 0.5f);
    }

    blockPeaks.fill (0.0f);
    overheadPeak = roomPeak = masterPeak = 0.0f;

    const int total = main.getNumSamples();
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
        mixChunk (main, aux, chunkStart, chunkLength, mix);
    }

    for (int s = 0; s < numSlots; ++s)
        meters.slots[(size_t) s].store (blockPeaks[(size_t) s], std::memory_order_relaxed);
    meters.overhead.store (overheadPeak, std::memory_order_relaxed);
    meters.room.store (roomPeak, std::memory_order_relaxed);
    meters.master.store (masterPeak, std::memory_order_relaxed);
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
    const float tuneCents = mix.strips[(size_t) target.slot].tune * 100.0f;

    // Kits with real round robins use them; otherwise vary the hit ourselves,
    // never picking the same variant twice in a row.
    bool hasRealRoundRobin = false;
    for (const auto& layer : *layers)
        hasRealRoundRobin = hasRealRoundRobin || layer.seqLength > 1;

    Variant variant = variants[0];
    if (! hasRealRoundRobin && mix.variation > 0.01f)
    {
        auto& last = lastVariant[(size_t) target.slot][(size_t) target.articulation];
        int pick = random.nextInt (3);
        if (pick >= last)
            ++pick;
        last = pick;

        variant = variants[pick];
        variant.cents *= mix.variation;
        variant.gainDb *= mix.variation;
        variant.offsetMs *= mix.variation;
        if (random.nextFloat() > mix.variation)
            variant.layerShift = 0;
    }

    // Velocity scales gain inside each layer; the layers carry the timbre.
    const float velocityGain = std::pow ((float) velocity / 127.0f, 1.6f);
    const int numLayers = (int) layers->size();

    for (int i = 0; i < numLayers; ++i)
    {
        const auto& candidate = (*layers)[(size_t) i];
        if (velocity < candidate.loVel || velocity > candidate.hiVel
            || randomValue < candidate.loRand || randomValue >= candidate.hiRand
            || (candidate.seqLength > 1 && (rr % candidate.seqLength) + 1 != candidate.seqPosition))
            continue;

        const int shifted = juce::jlimit (0, numLayers - 1, i + variant.layerShift);
        const auto& layer = (*layers)[(size_t) shifted];

        const float humanGainDb = (random.nextFloat() * 2.0f - 1.0f) * 3.0f * mix.humanize;
        const float humanCents = (random.nextFloat() * 2.0f - 1.0f) * 12.0f * mix.humanize;

        // A neighbouring layer is louder or softer: compensate so only the timbre changes
        const float layerCompensation = shifted > i ? -2.5f : shifted < i ? 2.5f : 0.0f;

        auto& v = allocateVoice();
        v.sample = layer.sample.get();
        v.slot = target.slot;
        v.position = variant.offsetMs * 0.001 * layer.sample->sampleRate;
        v.increment = layer.sample->sampleRate / sampleRate
                      * std::pow (2.0, (layer.tuneCents + tuneCents + humanCents + variant.cents) / 1200.0);
        v.gain = velocityGain * layer.gain * juce::Decibels::decibelsToGain (humanGainDb + variant.gainDb + layerCompensation);
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

void DrumEngine::processSlotFx (int slot, int numSamples, const MixSettings::Strip& strip)
{
    auto& fx = slotFx[(size_t) slot];
    auto& buffer = slotBuffers[(size_t) slot];
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);

    if (strip.eqLow != fx.lastLow)
    {
        fx.lastLow = strip.eqLow;
        fx.low.setLowShelf (sampleRate, 110.0, strip.eqLow);
    }
    if (strip.eqMid != fx.lastMid || strip.eqMidFreq != fx.lastMidFreq)
    {
        fx.lastMid = strip.eqMid;
        fx.lastMidFreq = strip.eqMidFreq;
        fx.mid.setPeak (sampleRate, strip.eqMidFreq, 1.0, strip.eqMid);
    }
    if (strip.eqHigh != fx.lastHigh)
    {
        fx.lastHigh = strip.eqHigh;
        fx.high.setHighShelf (sampleRate, 7500.0, strip.eqHigh);
    }
    if (strip.comp != fx.lastComp)
    {
        fx.lastComp = strip.comp;
        const float ratio = 1.0f + strip.comp * 5.0f;
        const float threshold = -strip.comp * 30.0f;
        fx.comp.setParameters (threshold, ratio, 8.0f, 90.0f, -threshold * (1.0f - 1.0f / ratio) * 0.5f);
    }

    fx.low.process (l, r, numSamples);
    fx.mid.process (l, r, numSamples);
    fx.high.process (l, r, numSamples);
    fx.transient.process (l, r, numSamples, strip.attack, strip.sustain);
    if (strip.comp > 0.01f)
        fx.comp.process (l, r, numSamples);
    dsp::saturate (l, r, numSamples, strip.drive);
}

void DrumEngine::mixChunk (juce::AudioBuffer<float>& main, const std::array<juce::AudioBuffer<float>*, numSlots>& aux,
                           int startSample, int numSamples, const MixSettings& mix)
{
    mainChunk.clear (0, numSamples);
    roomBuffer.clear (0, numSamples);
    overheadBuffer.clear (0, numSamples);

    bool anySolo = false;
    for (const auto& s : mix.strips)
        anySolo = anySolo || s.solo;

    for (int c = 0; c < numSlots; ++c)
    {
        const auto& strip = mix.strips[(size_t) c];
        processSlotFx (c, numSamples, strip);

        const bool audible = ! strip.mute && (! anySolo || strip.solo);
        const float level = audible ? juce::Decibels::decibelsToGain (strip.levelDb, -60.0f) : 0.0f;

        float panL, panR;
        balance (strip.pan, panL, panR);

        const float targetL = level * panL;
        const float targetR = level * panR;
        const float targetRoom = level * strip.room;
        const float targetOverhead = level * strip.overhead;
        const auto& src = slotBuffers[(size_t) c];

        // Separate outputs carry the dry piece; the mics still feed the main mix
        auto* separate = strip.separateOut ? aux[(size_t) c] : nullptr;
        if (separate != nullptr && separate->getNumChannels() > 0)
        {
            const int last = separate->getNumChannels() - 1;
            separate->addFromWithRamp (0, startSample, src.getReadPointer (0), numSamples, lastL[(size_t) c], targetL);
            separate->addFromWithRamp (juce::jmin (1, last), startSample, src.getReadPointer (1), numSamples, lastR[(size_t) c], targetR);
        }
        else
        {
            mainChunk.addFromWithRamp (0, 0, src.getReadPointer (0), numSamples, lastL[(size_t) c], targetL);
            mainChunk.addFromWithRamp (1, 0, src.getReadPointer (1), numSamples, lastR[(size_t) c], targetR);
        }

        roomBuffer.addFromWithRamp (0, 0, src.getReadPointer (0), numSamples, lastRoomSend[(size_t) c], targetRoom);
        roomBuffer.addFromWithRamp (1, 0, src.getReadPointer (1), numSamples, lastRoomSend[(size_t) c], targetRoom);

        // Overheads hear the kit from above, keeping each piece's position
        overheadBuffer.addFromWithRamp (0, 0, src.getReadPointer (0), numSamples, lastOverheadSend[(size_t) c] * panL, targetOverhead * panL);
        overheadBuffer.addFromWithRamp (1, 0, src.getReadPointer (1), numSamples, lastOverheadSend[(size_t) c] * panR, targetOverhead * panR);

        blockPeaks[(size_t) c] = juce::jmax (blockPeaks[(size_t) c], src.getMagnitude (0, numSamples) * juce::jmax (targetL, targetR));

        lastL[(size_t) c] = targetL;
        lastR[(size_t) c] = targetR;
        lastRoomSend[(size_t) c] = targetRoom;
        lastOverheadSend[(size_t) c] = targetOverhead;
    }

    // Overhead bus
    overheadLowCut.process (overheadBuffer.getWritePointer (0), overheadBuffer.getWritePointer (1), numSamples);
    overheadAir.process (overheadBuffer.getWritePointer (0), overheadBuffer.getWritePointer (1), numSamples);
    const float overheadLevel = mix.overheadMute ? 0.0f : juce::Decibels::decibelsToGain (mix.overheadDb, -60.0f);
    overheadPeak = juce::jmax (overheadPeak, overheadBuffer.getMagnitude (0, numSamples) * overheadLevel);
    mainChunk.addFromWithRamp (0, 0, overheadBuffer.getReadPointer (0), numSamples, lastOverhead, overheadLevel);
    mainChunk.addFromWithRamp (1, 0, overheadBuffer.getReadPointer (1), numSamples, lastOverhead, overheadLevel);
    lastOverhead = overheadLevel;

    // Room bus: reverb squashed by its own compressor
    reverb.processStereo (roomBuffer.getWritePointer (0), roomBuffer.getWritePointer (1), numSamples);
    if (mix.roomComp > 0.01f)
        roomCompressor.process (roomBuffer.getWritePointer (0), roomBuffer.getWritePointer (1), numSamples);
    const float roomLevel = mix.roomMute ? 0.0f : juce::Decibels::decibelsToGain (mix.roomDb, -60.0f);
    roomPeak = juce::jmax (roomPeak, roomBuffer.getMagnitude (0, numSamples) * roomLevel);
    mainChunk.addFromWithRamp (0, 0, roomBuffer.getReadPointer (0), numSamples, lastRoom, roomLevel);
    mainChunk.addFromWithRamp (1, 0, roomBuffer.getReadPointer (1), numSamples, lastRoom, roomLevel);
    lastRoom = roomLevel;

    // Master: glue compressor, level, soft limiter
    float* l = mainChunk.getWritePointer (0);
    float* r = mainChunk.getWritePointer (1);
    if (mix.glue > 0.01f)
        glueCompressor.process (l, r, numSamples);

    const float master = juce::Decibels::decibelsToGain (mix.masterDb, -60.0f);
    mainChunk.applyGainRamp (0, numSamples, lastMaster, master);
    lastMaster = master;

    for (int i = 0; i < numSamples; ++i)
    {
        l[i] = dsp::softLimit (l[i]);
        r[i] = dsp::softLimit (r[i]);
    }

    masterPeak = juce::jmax (masterPeak, mainChunk.getMagnitude (0, numSamples));

    if (main.getNumChannels() > 1)
    {
        main.copyFrom (0, startSample, mainChunk, 0, 0, numSamples);
        main.copyFrom (1, startSample, mainChunk, 1, 0, numSamples);
    }
    else if (main.getNumChannels() == 1)
    {
        main.copyFrom (0, startSample, mainChunk, 0, 0, numSamples);
        main.addFrom (0, startSample, mainChunk, 1, 0, numSamples);
        main.applyGain (0, startSample, numSamples, 0.5f);
    }
}
} // namespace woody
