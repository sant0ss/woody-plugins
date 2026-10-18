#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace eqwoody
{
// Lock-free hand-over of audio from the audio thread to the editor.
class AnalyzerFifo
{
public:
    AnalyzerFifo() : fifo (capacity), data ((size_t) capacity, 0.0f) {}

    // Pushes the mono sum of the pair; drops samples when the editor is not keeping up.
    void push (const float* left, const float* right, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            data[(size_t) (start1 + i)] = 0.5f * (left[i] + right[i]);
        for (int i = 0; i < size2; ++i)
            data[(size_t) (start2 + i)] = 0.5f * (left[size1 + i] + right[size1 + i]);

        fifo.finishedWrite (size1 + size2);
    }

    int pull (float* destination, int maxCount)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (maxCount, start1, size1, start2, size2);

        std::copy_n (data.begin() + start1, size1, destination);
        std::copy_n (data.begin() + start2, size2, destination + size1);

        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

private:
    static constexpr int capacity = 1 << 15;
    juce::AbstractFifo fifo;
    std::vector<float> data;
};

// Turns the latest audio into a smoothed magnitude spectrum in dBFS.
class Spectrum
{
public:
    static constexpr int order = 12;
    static constexpr int fftSize = 1 << order;
    static constexpr int numBins = fftSize / 2;

    Spectrum() : fft (order), window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        ring.fill (0.0f);
        smoothed.fill (-120.0f);
    }

    void update (AnalyzerFifo& fifo)
    {
        std::array<float, fftSize> incoming;
        const int count = fifo.pull (incoming.data(), fftSize);

        if (count > 0)
        {
            std::copy (ring.begin() + count, ring.end(), ring.begin());
            std::copy_n (incoming.begin(), count, ring.end() - count);
        }
        else
        {
            // Silence: let the display fall
            for (auto& v : smoothed)
                v -= 1.5f;
            return;
        }

        std::copy (ring.begin(), ring.end(), work.begin());
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        window.multiplyWithWindowingTable (work.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (work.data());

        // A full-scale sine reads about fftSize / 4 here
        const float reference = (float) fftSize * 0.25f;
        for (int k = 0; k < numBins; ++k)
        {
            const float db = juce::Decibels::gainToDecibels (work[(size_t) k] / reference, -120.0f);
            const float rate = db > smoothed[(size_t) k] ? 0.5f : 0.12f;
            smoothed[(size_t) k] += (db - smoothed[(size_t) k]) * rate;
        }
    }

    float dbAt (int bin) const { return smoothed[(size_t) juce::jlimit (0, numBins - 1, bin)]; }

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> ring;
    std::array<float, fftSize * 2> work {};
    std::array<float, numBins> smoothed;
};
} // namespace eqwoody
