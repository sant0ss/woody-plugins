#pragma once

#include <array>
#include "PluginProcessor.h"
#include "WoodyWidgets.h"

namespace compwoody
{
inline const juce::Colour blueColour { 0xff5b8cff };

juce::Colour bandColour (int band);
juce::String bandName (int band);

// Vertical gain-reduction meter: fills downward from the top
class GrBar : public juce::Component
{
public:
    void setReduction (float db);
    float getReduction() const { return level; }
    void paint (juce::Graphics&) override;

    static constexpr float rangeDb = 24.0f;

private:
    float level = 0.0f;
};

// The compressor's static curve (input level to output level) with a dot that follows the input
class TransferCurve : public juce::Component,
                      private juce::Timer
{
public:
    explicit TransferCurve (CompProcessor&);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    CompProcessor& processor;
    float inputDb = -120.0f;
};

// Scrolling history of the gain reduction, newest on the right
class HistoryView : public juce::Component,
                    private juce::Timer
{
public:
    explicit HistoryView (CompProcessor&);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    static constexpr int numPoints = 320;

    CompProcessor& processor;
    std::array<float, numPoints> history {};
    int position = 0;
};

// Two-segment switch between single band and multiband
class ModeSwitch : public juce::Component
{
public:
    explicit ModeSwitch (juce::AudioProcessorValueTreeState&);

    std::function<void (bool multiband)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    bool multiband = false;
    juce::ParameterAttachment attachment;
};

// Frequency axis with the four bands and draggable crossover handles; each band fills from the top with its reduction
class FreqStrip : public juce::Component,
                  private juce::Timer
{
public:
    explicit FreqStrip (CompProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    float freqToX (float freq) const;
    float xToFreq (float x) const;
    std::array<float, compwoody::numCrossovers> crossovers() const;
    int findHandle (float x) const;
    juce::RangedAudioParameter* parameter (int handle) const;

    CompProcessor& processor;
    int hovered = -1, dragging = -1;
    std::array<float, compwoody::numBands> shownReduction {};
};
} // namespace compwoody
