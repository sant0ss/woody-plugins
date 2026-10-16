#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace woody
{
namespace colours
{
inline const juce::Colour background { 0xff22170f };
inline const juce::Colour panel      { 0xff33241a };
inline const juce::Colour panelLight { 0xff4a3426 };
inline const juce::Colour accent     { 0xffe0a253 };
inline const juce::Colour text       { 0xfff3e6d3 };
inline const juce::Colour textDim    { 0xffa8927b };
inline const juce::Colour mute       { 0xffc8553d };
} // namespace colours

// UI text with accents (sources are compiled as UTF-8)
inline juce::String utf8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}

// Shared look for every Woody plugin: dark wood panels with amber accents.
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
};
} // namespace woody
