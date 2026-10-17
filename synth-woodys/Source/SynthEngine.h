#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "Dsp.h"
#include "Wavetables.h"

namespace synthwoody
{
constexpr int maxUnison = 7;
constexpr int maxVoices = 16;

struct ModSlot
{
    int src = 0;      // 0 none, 1 LFO 1, 2 LFO 2, 3 filter env, 4 velocity, 5 mod wheel
    int dst = 0;      // 0 cutoff, 1 resonance, 2 pitch, 3 wavetable position, 4 pulse width, 5 osc A level, 6 osc B level, 7 volume
    float amount = 0.0f;
};

// Everything the voices read, filled from the parameters once per block.
struct Settings
{
    int aShape = 0;
    float aPw = 0.5f, aSemi = 0.0f, aFine = 0.0f, aLevel = 0.7f;
    int bTable = 0;
    float bPos = 0.0f, bSemi = 0.0f, bFine = 0.0f, bLevel = 0.5f;

    int unison = 1;
    float uniDetune = 0.25f, uniSpread = 0.5f;
    float subLevel = 0.0f, noiseLevel = 0.0f;

    int fType = 0;
    float cutoff = 3000.0f, res = 0.2f, drive = 0.1f, envAmt = 0.3f, keytrack = 0.3f;

    struct EnvTimes { float a, d, s, r; };
    EnvTimes amp { 0.01f, 0.4f, 0.7f, 0.3f };
    EnvTimes flt { 0.01f, 0.5f, 0.3f, 0.4f };

    struct Lfo { int shape = 0; float rate = 2.0f; };
    std::array<Lfo, 2> lfo;
    std::array<ModSlot, 4> mod;
};

// Per-block values shared by all voices.
struct VoiceContext
{
    double sampleRate = 44100.0;
    float bendSemis = 0.0f;
    float modWheel = 0.0f;
    dsp::EnvCoefs ampCoefs, fltCoefs;
    const WavetableBank* bank = nullptr;
};

class Voice
{
public:
    bool isActive() const { return active; }
    bool isHeld() const { return held; }
    int getNote() const { return note; }
    juce::uint32 getAge() const { return age; }

    void start (int noteNumber, float velocity01, juce::uint32 newAge, const VoiceContext&);
    void release();
    void kill();

    // Adds this voice to the buffers.
    void render (float* left, float* right, int numSamples, const Settings&, const VoiceContext&);

    bool sustainPending = false; // note was released while the sustain pedal was down

private:
    static constexpr int controlRate = 8; // samples between modulation updates

    void updateControls (const Settings&, const VoiceContext&);
    float nextRandom();

    bool active = false, held = false;
    int note = 60;
    float velocity = 1.0f;
    juce::uint32 age = 0;
    int numUnison = 1;
    int controlCounter = 0;

    dsp::Envelope ampEnv, fltEnv;
    dsp::Ladder ladderL, ladderR;
    dsp::LadderCoefs ladderCoefs;

    std::array<float, maxUnison> phaseA {}, phaseB {};
    std::array<float, maxUnison> ratio {}, gainL {}, gainR {};
    float subPhase = 0.0f;

    float incA = 0.0f, incB = 0.0f, incSub = 0.0f;
    int mipA = 0, mipB = 0;
    float pulseWidth = 0.5f, levelA = 0.0f, levelB = 0.0f, wtPos = 0.0f, ampMod = 1.0f;
    float uniNorm = 1.0f;

    std::array<float, 2> lfoPhase {}, lfoHold {};
    juce::uint32 rngState = 22222u;
};

class Synth
{
public:
    void prepare (double sampleRate);
    void reset();

    // Renders into a stereo buffer (adds to it, so clear it first).
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const Settings&);

private:
    void handleMessage (const juce::MidiMessage&, const Settings&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void render (juce::AudioBuffer<float>&, int start, int count, const Settings&);

    std::array<Voice, maxVoices> voices;
    VoiceContext context;
    juce::uint32 ageCounter = 0;
    bool sustainDown = false;
};
} // namespace synthwoody
