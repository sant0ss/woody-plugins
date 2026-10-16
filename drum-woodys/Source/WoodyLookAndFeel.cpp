#include "WoodyLookAndFeel.h"

namespace woody
{
WoodyLookAndFeel::WoodyLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::buttonOnColourId, colours::mute);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::thumbColourId, colours::accent);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, colours::background);

    setColour (juce::ComboBox::backgroundColourId, colours::background);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::panelLight);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::ListBox::backgroundColourId, colours::background);
    setColour (juce::ListBox::outlineColourId, colours::panelLight);
    setColour (juce::TextEditor::backgroundColourId, colours::background);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::panelLight);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::TabbedComponent::backgroundColourId, colours::background);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::tabTextColourId, colours::textDim);
    setColour (juce::TabbedButtonBar::frontTextColourId, colours::accent);
    setColour (juce::ScrollBar::thumbColourId, colours::panelLight);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panelLight);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
}

void WoodyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    // Bipolar knobs (pan, tune) fill from the centre
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float originAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float lineWidth = juce::jmax (2.0f, radius * 0.18f);
    const float arcRadius = radius - lineWidth / 2.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::background);
    g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);
    g.setColour (colours::accent);
    g.strokePath (value, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto tip = centre.getPointOnCircumference (arcRadius - lineWidth, angle);
    g.setColour (colours::text);
    g.drawLine ({ centre, tip }, 2.0f);
}

void WoodyLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const float trackWidth = 6.0f;
    const float cx = (float) x + (float) width / 2.0f;
    const juce::Rectangle<float> track (cx - trackWidth / 2.0f, (float) y, trackWidth, (float) height);

    g.setColour (colours::background);
    g.fillRoundedRectangle (track, 3.0f);

    g.setColour (colours::accent);
    g.fillRoundedRectangle (track.withTop (sliderPos), 3.0f);

    const juce::Rectangle<float> thumb (cx - 14.0f, sliderPos - 5.0f, 28.0f, 10.0f);
    g.setColour (colours::text);
    g.fillRoundedRectangle (thumb, 3.0f);
}

void WoodyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool isHighlighted, bool isDown)
{
    auto colour = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;
    if (isDown)
        colour = colour.darker (0.2f);
    else if (isHighlighted)
        colour = colour.brighter (0.1f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f), 5.0f);
}
} // namespace woody
