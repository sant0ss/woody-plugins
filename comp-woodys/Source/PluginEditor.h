#pragma once

#include <array>
#include <memory>
#include "CompWidgets.h"
#include "PluginProcessor.h"
#include "WoodyWidgets.h"

namespace compwoody
{
// One multiband band: its knobs, solo and bypass buttons and a gain-reduction meter
class BandStrip : public juce::Component
{
public:
    BandStrip (CompProcessor&, int band);

    void update(); // meter, called from a timer

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    CompProcessor& processor;
    int band;

    woody::ParamKnob threshold, ratio, attack, release, makeup;
    juce::TextButton soloButton { "Solo" }, bypassButton { "Bypass" };
    ButtonAttachment soloAttachment, bypassAttachment;
    GrBar meter;
    juce::Label reductionLabel;
};
} // namespace compwoody

// Everything inside the plugin window, laid out at a fixed design size and scaled by the editor.
class CompView : public juce::Component,
                 private juce::Timer
{
public:
    static constexpr int designWidth = 1100;
    static constexpr int designHeight = 700;

    explicit CompView (CompProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Group
    {
        juce::Rectangle<int> area;
        juce::String title;
    };

    struct LabelledBox
    {
        juce::Label caption;
        juce::ComboBox box;
        std::unique_ptr<ComboAttachment> attachment;
    };

    void timerCallback() override;
    void setMode (bool multiband);
    void setupBox (LabelledBox&, const juce::String& caption, const juce::StringArray& items, const juce::String& paramId, const juce::String& tip);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<ButtonAttachment>&, const juce::String& text, const juce::String& paramId, const juce::String& tip);
    void updateDimming();

    CompProcessor& processor;
    bool multiband = false;

    juce::Label title, latencyLabel;
    compwoody::ModeSwitch modeSwitch;
    juce::ToggleButton bypassToggle;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    // Single band
    compwoody::TransferCurve curve;
    compwoody::HistoryView history;
    woody::PeakBar inputMeter { true }, outputMeter { true };
    compwoody::GrBar grMeter;
    juce::Label inputLabel, outputLabel, grLabel, inputCaption, outputCaption, grCaption;
    float inputHold = 0.0f, outputHold = 0.0f;

    woody::ParamKnob threshold, ratio, knee, attack, release, makeup, mix, scHighPass, link;
    juce::ToggleButton autoRelease, autoMakeup;
    std::unique_ptr<ButtonAttachment> autoReleaseAttachment, autoMakeupAttachment;
    LabelledBox detector, lookahead, channel;

    // Multiband
    compwoody::FreqStrip freqStrip;
    woody::ParamKnob mixM, kneeM, linkM;
    LabelledBox detectorM, channelM;
    std::array<std::unique_ptr<compwoody::BandStrip>, compwoody::numBands> strips;

    std::vector<juce::Component*> singleComponents, multiComponents;
    std::vector<Group> singleGroups, multiGroups;
    bool lastAutoRelease = false, lastAutoMakeup = false;
};

class CompEditor : public juce::AudioProcessorEditor
{
public:
    explicit CompEditor (CompProcessor&);
    ~CompEditor() override;

    void resized() override;

private:
    CompProcessor& processor;
    woody::WoodyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    CompView view;
    bool sized = false; // ignore resizes while the constructor sets things up

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompEditor)
};
