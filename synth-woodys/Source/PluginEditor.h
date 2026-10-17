#pragma once

#include "Components.h"
#include "PluginProcessor.h"

// Everything inside the plugin window, laid out at a fixed design size and scaled by the editor.
class SynthView : public juce::Component,
                  private juce::Timer
{
public:
    static constexpr int designWidth = 1180;
    static constexpr int designHeight = 740;

    SynthView (SynthWoodysProcessor&, juce::LookAndFeel&);
    ~SynthView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        juce::Colour colour;
    };

    void timerCallback() override;
    void addPanel (const char* utf8Title, juce::Rectangle<int> bounds, juce::Colour);
    synthwoody::Knob& addKnob (synthwoody::ParamIndex, const juce::String& caption, juce::Colour);
    juce::ComboBox& addCombo (synthwoody::ParamIndex);
    juce::Slider& addAmountSlider (synthwoody::ParamIndex);
    void refreshPresets();
    void savePreset();
    void deletePreset();

    SynthWoodysProcessor& processor;
    juce::LookAndFeel& lookAndFeel;

    std::vector<Panel> panels;
    std::array<std::unique_ptr<synthwoody::Knob>, synthwoody::numParams> knobs;
    std::array<std::unique_ptr<juce::ComboBox>, synthwoody::numParams> combos;
    std::array<std::unique_ptr<ComboAttachment>, synthwoody::numParams> comboAttachments;
    std::array<std::unique_ptr<juce::Slider>, synthwoody::numParams> amountSliders;
    std::array<std::unique_ptr<SliderAttachment>, synthwoody::numParams> sliderAttachments;

    juce::Label title, subtitle;
    juce::ComboBox presetBox;
    juce::String shownPreset; // the name the preset box was last built for
    juce::TextButton saveButton { "Salvar" };
    juce::TextButton deleteButton { "Excluir" };
    juce::TextButton panicButton { utf8Text ("Silenciar") };
    std::vector<juce::ValueTree> userPresets;

    synthwoody::AnalogWaveView analogView;
    synthwoody::WavetableView wavetableView;
    synthwoody::FilterView filterView;
    synthwoody::EnvelopeView ampEnvView, fltEnvView;

    juce::MidiKeyboardComponent keyboard;

    juce::Label lfo1Label, lfo2Label;
    static juce::String utf8Text (const char* text) { return juce::String::fromUTF8 (text); }
};

class SynthWoodysEditor : public juce::AudioProcessorEditor
{
public:
    explicit SynthWoodysEditor (SynthWoodysProcessor&);
    ~SynthWoodysEditor() override;

    void resized() override;

private:
    SynthWoodysProcessor& processor;
    woody::WoodyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    SynthView view;
    bool sized = false; // ignore resizes while the constructor sets things up

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthWoodysEditor)
};
