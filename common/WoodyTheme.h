#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace woody
{
namespace colours
{
inline const juce::Colour background { 0xff101218 };
inline const juce::Colour panel      { 0xff181b23 };
inline const juce::Colour panelLight { 0xff232733 };
inline const juce::Colour line       { 0xff2e3342 };
inline const juce::Colour text       { 0xffe8eaf0 };
inline const juce::Colour textDim    { 0xff8a90a2 };
inline const juce::Colour accent     { 0xffff8a3d };
inline const juce::Colour accent2    { 0xff3dd6c6 };
inline const juce::Colour mute       { 0xffff5470 };
inline const juce::Colour solo       { 0xffffd23d };
} // namespace colours

// UI text with accents (sources are compiled as UTF-8)
inline juce::String utf8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}

// Shared look for every Woody plugin: flat dark surfaces, pill buttons, thin controls.
class WoodyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WoodyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isHighlighted, bool isDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};
} // namespace woody
