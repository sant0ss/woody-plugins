#pragma once

#include "PluginProcessor.h"
#include "WoodyWidgets.h"

namespace satwoody
{
// The selected transfer curve (bias included) with a dot that follows the signal entering it.
class CurveView : public juce::Component,
                  private juce::Timer
{
public:
    explicit CurveView (SatProcessor&);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    SatProcessor& processor;
    std::array<float, 6> trail {};
};

// The last few milliseconds of the signal, before and after the saturation.
class ScopeView : public juce::Component,
                  private juce::Timer
{
public:
    explicit ScopeView (SatProcessor&);

    void paint (juce::Graphics&) override;

private:
    static constexpr int shown = 1024;

    void timerCallback() override;

    SatProcessor& processor;
    std::array<float, shown> dry {}, wet {};
};
} // namespace satwoody

// Everything inside the plugin window, laid out at a fixed design size and scaled by the editor.
class SatView : public juce::Component,
                private juce::Timer
{
public:
    static constexpr int designWidth = 960;
    static constexpr int designHeight = 600;

    explicit SatView (SatProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupCaption (juce::Label&, const juce::String& text);

    SatProcessor& processor;

    juce::Label title, latencyLabel, typeCaption, stereoCaption, inCaption, outCaption;
    juce::ComboBox typeBox, stereoBox, osBox;
    juce::ToggleButton bypassButton { "Bypass" }, autoButton { "Auto-gain" };

    woody::ParamKnob input, drive, tone, bias, focusLow, focusHigh, mix, output;
    woody::PeakBar inMeter { true }, outMeter { true };
    satwoody::CurveView curve;
    satwoody::ScopeView scope;

    std::unique_ptr<ComboAttachment> typeAttachment, stereoAttachment, osAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, autoAttachment;
    int shownLatency = -1;
};

class SatEditor : public juce::AudioProcessorEditor
{
public:
    explicit SatEditor (SatProcessor&);
    ~SatEditor() override;

    void resized() override;

private:
    SatProcessor& processor;
    woody::WoodyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    SatView view;
    bool sized = false; // ignore resizes while the constructor sets things up

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatEditor)
};
