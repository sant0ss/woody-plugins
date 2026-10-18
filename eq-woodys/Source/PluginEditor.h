#pragma once

#include "Analyzer.h"
#include "PluginProcessor.h"
#include "WoodyWidgets.h"

namespace eqwoody
{
juce::Colour bandColour (int band);

// The frequency response: analyzer behind, every band's curve, the combined curve on top and a
// draggable node for each band. Drag a node to move it, scroll to change Q, double-click to add or remove.
class ResponseView : public juce::Component,
                     private juce::Timer
{
public:
    explicit ResponseView (EqProcessor&);

    std::function<void (int band)> onSelect;
    void setSelected (int band);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    float freqToX (float freq) const;
    float xToFreq (float x) const;
    float gainToY (float gainDb) const;
    float yToGain (float y) const;
    float rangeDb() const;
    juce::Point<float> nodePosition (const BandSettings&) const;
    int findNode (juce::Point<float>) const;
    void select (int band);
    void beginDrag (int band);
    void endDrag();
    void showMenu (int band);
    void setParam (const juce::String& id, float value);

    EqProcessor& processor;
    Spectrum preSpectrum, postSpectrum;

    int selected = 0, hovered = -1, dragging = -1;
    juce::Point<float> dragOffset;
    std::vector<juce::RangedAudioParameter*> gestureParams;
};

// A row of numbered chips, one per band.
class BandChips : public juce::Component
{
public:
    explicit BandChips (EqProcessor& p) : processor (p) {}

    std::function<void (int)> onSelect;
    void setSelected (int band) { selected = band; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    EqProcessor& processor;
    int selected = 0;
};

// Controls of the selected band.
class BandPanel : public juce::Component
{
public:
    explicit BandPanel (EqProcessor&);

    void setBand (int band);
    void refreshState(); // enables or disables controls according to the band type

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    EqProcessor& processor;
    int band = 0;

    juce::ToggleButton onButton;
    juce::ComboBox typeBox, slopeBox, placeBox, satModeBox;
    juce::Label typeLabel, slopeLabel, placeLabel, satModeLabel, titleLabel;
    std::unique_ptr<woody::ParamKnob> freq, gain, q, sat;
    std::unique_ptr<ButtonAttachment> onAttachment;
    std::unique_ptr<ComboAttachment> typeAttachment, slopeAttachment, placeAttachment, satModeAttachment;
};
} // namespace eqwoody

// Everything inside the plugin window, laid out at a fixed design size and scaled by the editor.
class EqView : public juce::Component,
               private juce::Timer
{
public:
    static constexpr int designWidth = 1100;
    static constexpr int designHeight = 700;

    explicit EqView (EqProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void select (int band);

    EqProcessor& processor;

    juce::Label title, latencyLabel;
    juce::ComboBox phaseBox, qualityBox, analyzerBox, rangeBox;
    juce::ToggleButton bypassButton;
    juce::TextButton resetButton { "Zerar tudo" };
    woody::ParamBar outBar;
    woody::PeakBar outMeter;
    std::unique_ptr<ComboAttachment> phaseAttachment, qualityAttachment, analyzerAttachment, rangeAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    eqwoody::ResponseView response;
    eqwoody::BandChips chips;
    eqwoody::BandPanel panel;
};

class EqEditor : public juce::AudioProcessorEditor
{
public:
    explicit EqEditor (EqProcessor&);
    ~EqEditor() override;

    void resized() override;

private:
    EqProcessor& processor;
    woody::WoodyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    EqView view;
    bool sized = false; // ignore resizes while the constructor sets things up

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqEditor)
};
