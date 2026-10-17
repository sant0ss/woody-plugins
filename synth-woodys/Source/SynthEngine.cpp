#include "SynthEngine.h"
#include <cmath>

namespace synthwoody
{
namespace
{
constexpr float twoPi = 6.28318530717958647692f;

// Analog oscillator: band-limited with polyBLEP (saw, square) or from the sine/triangle wavetable frames.
float analogSample (int shape, float phase, float dt, float pulseWidth, int mip, const WavetableBank& bank)
{
    switch (shape)
    {
        case 0: // saw
            return 2.0f * phase - 1.0f - dsp::polyBlep (phase, dt);

        case 1: // square / pulse
        {
            float v = phase < pulseWidth ? 1.0f : -1.0f;
            v += dsp::polyBlep (phase, dt);
            float shifted = phase + 1.0f - pulseWidth;
            shifted -= std::floor (shifted);
            v -= dsp::polyBlep (shifted, dt);
            return v;
        }

        case 2: // triangle
            return bank.read (0, 1.0f / (float) (numFrames - 1), mip, phase);

        default: // sine
            return bank.read (0, 0.0f, mip, phase);
    }
}

float lfoShapeValue (int shape, float phase, float held)
{
    switch (shape)
    {
        case 0:  return std::sin (twoPi * phase);
        case 1:  return 1.0f - 4.0f * std::abs (phase - 0.5f);
        case 2:  return 2.0f * phase - 1.0f;
        case 3:  return phase < 0.5f ? 1.0f : -1.0f;
        default: return held;
    }
}
} // namespace

//==============================================================================
float Voice::nextRandom()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float) (rngState & 0xffffff) / (float) 0x7fffff - 1.0f; // -1..1
}

void Voice::start (int noteNumber, float velocity01, juce::uint32 newAge, const VoiceContext&)
{
    note = noteNumber;
    velocity = velocity01;
    age = newAge;
    active = true;
    held = true;
    sustainPending = false;
    controlCounter = 0;
    subPhase = 0.0f;
    lfoPhase = { 0.0f, 0.0f };
    rngState = 0x9e3779b9u ^ (newAge * 2654435761u);
    lfoHold = { nextRandom(), nextRandom() };

    ampEnv.trigger();
    fltEnv.trigger();
    ladderL.reset();
    ladderR.reset();

    for (int u = 0; u < maxUnison; ++u)
    {
        // The first unison voice starts in phase so the attack is consistent
        phaseA[(size_t) u] = u == 0 ? 0.0f : (nextRandom() * 0.5f + 0.5f);
        phaseB[(size_t) u] = u == 0 ? 0.0f : (nextRandom() * 0.5f + 0.5f);
    }
}

void Voice::release()
{
    held = false;
    ampEnv.release();
    fltEnv.release();
}

void Voice::kill()
{
    active = false;
    held = false;
    ampEnv = {};
    fltEnv = {};
}

void Voice::updateControls (const Settings& s, const VoiceContext& c)
{
    const float sr = (float) c.sampleRate;

    // LFOs (bipolar)
    std::array<float, 2> lfoValue {};
    for (size_t k = 0; k < 2; ++k)
    {
        lfoValue[k] = lfoShapeValue (s.lfo[k].shape, lfoPhase[k], lfoHold[k]);
        lfoPhase[k] += s.lfo[k].rate * (float) controlRate / sr;
        if (lfoPhase[k] >= 1.0f)
        {
            lfoPhase[k] -= std::floor (lfoPhase[k]);
            lfoHold[k] = nextRandom();
        }
    }

    // Modulation matrix
    const float sources[6] = { 0.0f, lfoValue[0], lfoValue[1], fltEnv.level, velocity, c.modWheel };
    float acc[8] = {};
    for (const auto& slot : s.mod)
        if (slot.src > 0 && slot.amount != 0.0f)
            acc[slot.dst] += sources[slot.src] * slot.amount;

    // Pitch
    const float pitchSemis = c.bendSemis + acc[2] * 12.0f;
    const float base = (float) (note - 69) + pitchSemis;
    const float freqA = 440.0f * std::exp2 ((base + s.aSemi + s.aFine / 100.0f) / 12.0f);
    const float freqB = 440.0f * std::exp2 ((base + s.bSemi + s.bFine / 100.0f) / 12.0f);
    const float freqSub = 440.0f * std::exp2 ((base - 12.0f) / 12.0f);

    incA = freqA / sr;
    incB = freqB / sr;
    incSub = freqSub / sr;
    mipA = WavetableBank::mipFor (freqA * 1.04f, sr);
    mipB = WavetableBank::mipFor (freqB * 1.04f, sr);

    pulseWidth = juce::jlimit (0.05f, 0.95f, s.aPw + acc[4] * 0.45f);
    wtPos = juce::jlimit (0.0f, 1.0f, s.bPos + acc[3]);
    levelA = juce::jmax (0.0f, s.aLevel + acc[5]);
    levelB = juce::jmax (0.0f, s.bLevel + acc[6]);
    ampMod = juce::jlimit (0.0f, 2.0f, 1.0f + acc[7]);

    // Unison spread
    numUnison = juce::jlimit (1, maxUnison, s.unison);
    uniNorm = 1.0f / std::sqrt ((float) numUnison);
    for (int u = 0; u < numUnison; ++u)
    {
        const float pos = numUnison == 1 ? 0.0f : 2.0f * (float) u / (float) (numUnison - 1) - 1.0f;
        ratio[(size_t) u] = std::exp2 (pos * s.uniDetune * 50.0f / 1200.0f);

        const float pan = pos * s.uniSpread;
        gainL[(size_t) u] = std::sqrt (0.5f * (1.0f - pan));
        gainR[(size_t) u] = std::sqrt (0.5f * (1.0f + pan));
    }

    // Filter
    const float cutoffOctaves = s.envAmt * fltEnv.level * 6.0f
                                + (float) (note - 60) / 12.0f * s.keytrack
                                + acc[0] * 5.0f;
    const float cutoffHz = s.cutoff * std::exp2 (cutoffOctaves);
    const float resonance = juce::jlimit (0.0f, 1.0f, s.res + acc[1] * 0.5f);
    ladderCoefs = dsp::makeLadderCoefs (cutoffHz, resonance, s.drive, sr);
}

void Voice::render (float* left, float* right, int numSamples, const Settings& s, const VoiceContext& c)
{
    const WavetableBank& bank = *c.bank;
    const float velocityGain = 0.2f + 0.8f * velocity;

    for (int i = 0; i < numSamples; ++i)
    {
        const float ampLevel = ampEnv.tick (c.ampCoefs);
        fltEnv.tick (c.fltCoefs);

        if (ampEnv.stage == dsp::Envelope::Stage::idle)
        {
            active = false;
            return;
        }

        if (controlCounter == 0)
            updateControls (s, c);
        controlCounter = (controlCounter + 1) % controlRate;

        float l = 0.0f, r = 0.0f;

        // Oscillator A (analog) and B (wavetable), both unison-stacked
        for (int u = 0; u < numUnison; ++u)
        {
            const auto ui = (size_t) u;

            if (levelA > 0.0f)
            {
                const float dt = incA * ratio[ui];
                const float v = analogSample (s.aShape, phaseA[ui], dt, pulseWidth, mipA, bank) * levelA;
                l += v * gainL[ui];
                r += v * gainR[ui];
                phaseA[ui] += dt;
                phaseA[ui] -= std::floor (phaseA[ui]);
            }

            if (levelB > 0.0f)
            {
                const float dt = incB * ratio[ui];
                const float v = bank.read (s.bTable, wtPos, mipB, phaseB[ui]) * levelB;
                l += v * gainL[ui];
                r += v * gainR[ui];
                phaseB[ui] += dt;
                phaseB[ui] -= std::floor (phaseB[ui]);
            }
        }

        l *= uniNorm;
        r *= uniNorm;

        // Sub oscillator and noise are mono and centred
        if (s.subLevel > 0.0f)
        {
            const float sub = std::sin (twoPi * subPhase) * s.subLevel;
            l += sub;
            r += sub;
            subPhase += incSub;
            subPhase -= std::floor (subPhase);
        }

        if (s.noiseLevel > 0.0f)
        {
            const float n = nextRandom() * s.noiseLevel * 0.6f;
            l += n;
            r += n;
        }

        l *= 0.6f;
        r *= 0.6f;

        const float gain = ampLevel * velocityGain * ampMod * 0.7f;
        left[i] += ladderL.process (l, ladderCoefs, s.fType) * gain;
        right[i] += ladderR.process (r, ladderCoefs, s.fType) * gain;
    }
}

//==============================================================================
void Synth::prepare (double sampleRate)
{
    context.sampleRate = sampleRate;
    context.bank = &WavetableBank::get();
    reset();
}

void Synth::reset()
{
    for (auto& v : voices)
        v.kill();

    sustainDown = false;
}

void Synth::noteOn (int note, float velocity)
{
    Voice* target = nullptr;

    for (auto& v : voices)
        if (! v.isActive())
        {
            target = &v;
            break;
        }

    // All busy: take the oldest
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.getAge() < target->getAge())
                target = &v;
    }

    target->start (note, velocity, ++ageCounter, context);
}

void Synth::noteOff (int note)
{
    for (auto& v : voices)
    {
        if (! v.isActive() || ! v.isHeld() || v.getNote() != note)
            continue;

        if (sustainDown)
            v.sustainPending = true;
        else
            v.release();
    }
}

void Synth::handleMessage (const juce::MidiMessage& m, const Settings&)
{
    if (m.isNoteOn())
    {
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        noteOff (m.getNoteNumber());
    }
    else if (m.isPitchWheel())
    {
        context.bendSemis = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f; // +-2 semitones
    }
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1)
        {
            context.modWheel = (float) m.getControllerValue() / 127.0f;
        }
        else if (m.getControllerNumber() == 64)
        {
            sustainDown = m.getControllerValue() >= 64;
            if (! sustainDown)
                for (auto& v : voices)
                    if (v.isActive() && v.sustainPending)
                    {
                        v.sustainPending = false;
                        v.release();
                    }
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        reset();
    }
}

void Synth::render (juce::AudioBuffer<float>& buffer, int start, int count, const Settings& s)
{
    if (count <= 0)
        return;

    float* left = buffer.getWritePointer (0, start);
    float* right = buffer.getWritePointer (1, start);

    for (auto& v : voices)
        if (v.isActive())
            v.render (left, right, count, s, context);
}

void Synth::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const Settings& s)
{
    context.ampCoefs = dsp::makeEnvCoefs (s.amp.a, s.amp.d, s.amp.s, s.amp.r, context.sampleRate);
    context.fltCoefs = dsp::makeEnvCoefs (s.flt.a, s.flt.d, s.flt.s, s.flt.r, context.sampleRate);

    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPosition = juce::jlimit (position, buffer.getNumSamples(), metadata.samplePosition);
        render (buffer, position, eventPosition - position, s);
        position = eventPosition;
        handleMessage (metadata.getMessage(), s);
    }

    render (buffer, position, buffer.getNumSamples() - position, s);
}
} // namespace synthwoody
