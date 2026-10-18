#include "SatEngine.h"
#include "SatParams.h"

namespace satwoody
{
namespace
{
constexpr int maxDelay = 4096;
constexpr double toneTiltDb = 9.0;
constexpr double tiltPivotHz = 1000.0;
constexpr double dcCutoffHz = 10.0;
constexpr double autoGainSeconds = 0.15;
constexpr double smoothingSeconds = 0.02;

float toGain (double db) { return (float) std::pow (10.0, db / 20.0); }
} // namespace

void SatEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    for (size_t k = 0; k < oversamplers.size(); ++k)
    {
        oversamplers[k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, k + 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversamplers[k]->initProcessing ((size_t) maxBlock);
        latencies[k + 1] = juce::jlimit (0, maxDelay - 1, juce::roundToInt (oversamplers[k]->getLatencyInSamples()));
    }

    for (size_t c = 0; c < 2; ++c)
    {
        for (auto* v : { &dry[c], &delayedDry[c], &band[c], &delayedBand[c], &wetOut[c] })
            v->assign ((size_t) maxBlock, 0.0f);

        dryDelay[c].init (maxDelay);
        bandDelay[c].init (maxDelay);
    }

    driveGains.assign ((size_t) maxBlock, 1.0f);
    biasValues.assign ((size_t) maxBlock, 0.0f);

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
    for (auto* f : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
        f->prepare (spec);

    lowPass1.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowPass2.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highPass1.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highPass2.setType (juce::dsp::StateVariableTPTFilterType::highpass);

    for (auto* s : { &inputGain, &outputGain, &driveGain, &toneLow, &toneHigh })
        s->reset (sampleRate, smoothingSeconds);
    for (auto* s : { &mixAmount, &biasAmount, &autoAmount })
        s->reset (sampleRate, smoothingSeconds);

    for (auto& a : scopeDry)
        a.store (0.0f);
    for (auto& a : scopeWet)
        a.store (0.0f);

    reset();
}

void SatEngine::reset()
{
    for (size_t c = 0; c < 2; ++c)
    {
        dryDelay[c].init (maxDelay);
        bandDelay[c].init (maxDelay);
        dcBlockers[c].reset();
        tiltState[c] = 0.0f;
    }

    for (auto* f : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
        f->reset();

    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();

    resetSaturation();
    inputMs = outputMs = 0.0;
    needsSnap = true;
}

void SatEngine::resetSaturation()
{
    for (auto& s : saturators)
        s.reset();
}

int SatEngine::latencyFor (int oversamplingIndex) const
{
    return latencies[(size_t) juce::jlimit (0, 3, oversamplingIndex)];
}

void SatEngine::snapSmoothers (const Settings& s)
{
    inputGain.setCurrentAndTargetValue (s.bypass ? 1.0f : toGain (s.inputDb));
    outputGain.setCurrentAndTargetValue (s.bypass ? 1.0f : toGain (s.outputDb));
    driveGain.setCurrentAndTargetValue (toGain (s.drive * maxDriveDb));
    toneLow.setCurrentAndTargetValue (toGain (-s.tone * toneTiltDb));
    toneHigh.setCurrentAndTargetValue (toGain (s.tone * toneTiltDb));
    mixAmount.setCurrentAndTargetValue (s.bypass ? 0.0f : s.mix);
    biasAmount.setCurrentAndTargetValue (s.bias);
    autoAmount.setCurrentAndTargetValue (s.autoGain ? 1.0f : 0.0f);
    needsSnap = false;
}

void SatEngine::process (float* left, float* right, int numSamples, const Settings& settings)
{
    if (needsSnap)
        snapSmoothers (settings);

    for (int start = 0; start < numSamples; start += maxBlock)
        processChunk (left + start, right + start, juce::jmin (maxBlock, numSamples - start), settings);
}

void SatEngine::processChunk (float* left, float* right, int n, const Settings& s)
{
    inputGain.setTargetValue (s.bypass ? 1.0f : toGain (s.inputDb));
    outputGain.setTargetValue (s.bypass ? 1.0f : toGain (s.outputDb));
    driveGain.setTargetValue (toGain (s.drive * maxDriveDb));
    toneLow.setTargetValue (toGain (-s.tone * toneTiltDb));
    toneHigh.setTargetValue (toGain (s.tone * toneTiltDb));
    mixAmount.setTargetValue (s.bypass ? 0.0f : s.mix);
    biasAmount.setTargetValue (s.bias);
    autoAmount.setTargetValue (s.autoGain ? 1.0f : 0.0f);

    const int osIndex = juce::jlimit (0, 3, s.oversampling);
    if (osIndex != activeOversampling)
    {
        activeOversampling = osIndex;
        resetSaturation();
        if (osIndex > 0)
            oversamplers[(size_t) osIndex - 1]->reset();
    }
    const int latency = latencies[(size_t) osIndex];

    const bool midSide = s.stereo != Stereo;
    const std::array<bool, 2> saturated { s.stereo != SideOnly, s.stereo != MidOnly };
    const int numSaturated = (saturated[0] ? 1 : 0) + (saturated[1] ? 1 : 0);
    const int firstSaturated = saturated[0] ? 0 : 1;

    const float low = juce::jlimit (focusLowMin, focusLowMax, s.focusLow);
    const float high = juce::jmax (juce::jlimit (focusHighMin, focusHighMax, s.focusHigh), low * 1.25f);
    const bool focusActive = low > focusLowMin + 0.5f || high < focusHighMax - 1.0f;

    // Input trim, meter and mid/side encoding
    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inputGain.getNextValue();
        const float l = left[i] * g, r = right[i] * g;
        peakIn = juce::jmax (peakIn, std::abs (l), std::abs (r));

        dry[0][(size_t) i] = midSide ? 0.5f * (l + r) : l;
        dry[1][(size_t) i] = midSide ? 0.5f * (l - r) : r;
    }
    inputPeak = juce::jmax (peakIn, inputPeak.load() * 0.9f);

    // Focus split: the band is what gets saturated, the rest stays untouched
    if (focusActive)
    {
        const float maxCutoff = (float) sampleRate * 0.45f;
        lowPass1.setCutoffFrequency (juce::jmin (high, maxCutoff));
        lowPass2.setCutoffFrequency (juce::jmin (high, maxCutoff));
        highPass1.setCutoffFrequency (low);
        highPass2.setCutoffFrequency (low);
    }
    else
    {
        for (auto* f : { &lowPass1, &lowPass2, &highPass1, &highPass2 })
            f->reset();
    }

    for (int c = 0; c < 2; ++c)
    {
        const auto ch = (size_t) c;
        for (int i = 0; i < n; ++i)
        {
            const float x = dry[ch][(size_t) i];
            delayedDry[ch][(size_t) i] = dryDelay[ch].process (x, latency);

            if (! saturated[ch])
                continue;

            float v = x;
            if (focusActive)
            {
                v = highPass2.processSample (c, highPass1.processSample (c, lowPass2.processSample (c, lowPass1.processSample (c, x))));
                delayedBand[ch][(size_t) i] = bandDelay[ch].process (v, latency);
            }
            band[ch][(size_t) i] = v;
        }
    }

    // Drive and bias in front of the curve
    for (int i = 0; i < n; ++i)
    {
        driveGains[(size_t) i] = driveGain.getNextValue();
        biasValues[(size_t) i] = 0.5f * biasAmount.getNextValue();
    }

    // The curve works in place on the (possibly oversampled) band signal
    std::array<float*, 2> pointers { wetOut[0].data(), wetOut[1].data() };
    for (int c = 0; c < 2; ++c)
    {
        const auto ch = (size_t) c;
        for (int i = 0; i < n; ++i)
            wetOut[ch][(size_t) i] = saturated[ch] ? band[ch][(size_t) i] * driveGains[(size_t) i] + biasValues[(size_t) i] : 0.0f;
    }
    curveInput = wetOut[(size_t) firstSaturated][(size_t) n - 1];

    const auto mode = (woody::SatMode) juce::jlimit (0, 4, s.type);
    juce::dsp::AudioBlock<float> block (pointers.data(), 2, (size_t) n);

    if (osIndex == 0)
    {
        for (int c = 0; c < 2; ++c)
            if (saturated[(size_t) c])
                for (int i = 0; i < n; ++i)
                    wetOut[(size_t) c][(size_t) i] = saturators[(size_t) c].process (mode, wetOut[(size_t) c][(size_t) i]);
    }
    else
    {
        auto& os = *oversamplers[(size_t) osIndex - 1];
        auto up = os.processSamplesUp (block);

        for (int c = 0; c < 2; ++c)
            if (saturated[(size_t) c])
            {
                float* data = up.getChannelPointer ((size_t) c);
                for (size_t j = 0; j < up.getNumSamples(); ++j)
                    data[j] = saturators[(size_t) c].process (mode, data[j]);
            }

        os.processSamplesDown (block);
    }

    // Back at the normal rate: DC removal, tone, auto-gain, mix
    const bool removeDc = s.type == (int) woody::SatMode::Tube || std::abs (s.bias) > 0.001f;
    if (! removeDc)
        for (auto& d : dcBlockers)
            d.reset();

    const float dcCoefficient = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * dcCutoffHz / sampleRate);
    const float tiltCoefficient = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * tiltPivotHz / sampleRate));
    const float averaging = (float) (1.0 - std::exp (-1.0 / (autoGainSeconds * sampleRate)));

    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float lowGain = toneLow.getNextValue(), highGain = toneHigh.getNextValue();
        const float mix = mixAmount.getNextValue();
        const float autoWeight = autoAmount.getNextValue();

        double inSum = 0.0, outSum = 0.0;
        for (int c = 0; c < 2; ++c)
        {
            const auto ch = (size_t) c;
            if (! saturated[ch])
                continue;

            float v = wetOut[ch][(size_t) i];
            if (removeDc)
                v = dcBlockers[ch].process (v, dcCoefficient);

            tiltState[ch] += tiltCoefficient * (v - tiltState[ch]);
            v = tiltState[ch] * lowGain + (v - tiltState[ch]) * highGain;
            wetOut[ch][(size_t) i] = v;

            const double in = band[ch][(size_t) i];
            inSum += in * in;
            outSum += (double) v * v;
        }

        inputMs += averaging * (inSum / numSaturated - inputMs);
        outputMs += averaging * (outSum / numSaturated - outputMs);
        const float compensation = juce::jlimit (0.0631f, 15.85f, (float) std::sqrt ((inputMs + 1.0e-9) / (outputMs + 1.0e-9)));
        const float autoGain = 1.0f + autoWeight * (compensation - 1.0f);

        for (int c = 0; c < 2; ++c)
        {
            const auto ch = (size_t) c;
            const float d = delayedDry[ch][(size_t) i];
            if (! saturated[ch])
            {
                wetOut[ch][(size_t) i] = d;
                continue;
            }

            const float untouched = focusActive ? d - delayedBand[ch][(size_t) i] : 0.0f;
            const float wet = untouched + wetOut[ch][(size_t) i] * autoGain;
            wetOut[ch][(size_t) i] = d * (1.0f - mix) + wet * mix;

            if (c == firstSaturated)
            {
                const int position = scopePosition.load (std::memory_order_relaxed);
                scopeDry[(size_t) position].store (d, std::memory_order_relaxed);
                scopeWet[(size_t) position].store (wet, std::memory_order_relaxed);
                scopePosition.store ((position + 1) % scopeSize, std::memory_order_release);
            }
        }

        const float g = outputGain.getNextValue();
        const float a = wetOut[0][(size_t) i], b = wetOut[1][(size_t) i];
        const float l = (midSide ? a + b : a) * g;
        const float r = (midSide ? a - b : b) * g;
        left[i] = l;
        right[i] = r;
        peakOut = juce::jmax (peakOut, std::abs (l), std::abs (r));
    }

    outputPeak = juce::jmax (peakOut, outputPeak.load() * 0.9f);
}
} // namespace satwoody
