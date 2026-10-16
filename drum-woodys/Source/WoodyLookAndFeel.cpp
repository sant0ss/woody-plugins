#include "WoodyLookAndFeel.h"

namespace woody
{
WoodyLookAndFeel::WoodyLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, colours::panelLight);

    setColour (juce::ComboBox::backgroundColourId, colours::panelLight);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::ListBox::backgroundColourId, colours::panel);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, colours::panelLight);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, colours::accent);
    setColour (juce::ScrollBar::thumbColourId, colours::line);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panelLight);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::line);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::line);
    setColour (juce::ToggleButton::textColourId, colours::text);
}

void WoodyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float lineWidth = juce::jmax (2.0f, radius * 0.14f);
    const float arcRadius = radius - lineWidth / 2.0f;

    // Bipolar knobs (pan, tune, EQ...) fill from the centre
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float originAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::panelLight);
    g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);
    g.setColour (fill);
    g.strokePath (value, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // A dot instead of a pointer line
    const auto dot = centre.getPointOnCircumference (arcRadius - lineWidth * 1.8f, angle);
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (lineWidth * 1.3f, lineWidth * 1.3f).withCentre (dot));
}

void WoodyLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const float trackWidth = 4.0f;
    const auto fill = slider.findColour (juce::Slider::trackColourId);

    if (style == juce::Slider::LinearVertical)
    {
        const float cx = (float) x + (float) width / 2.0f;
        const juce::Rectangle<float> track (cx - trackWidth / 2.0f, (float) y, trackWidth, (float) height);
        g.setColour (colours::panelLight);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (track.withTop (sliderPos), 2.0f);
        g.setColour (colours::text);
        g.fillRoundedRectangle (juce::Rectangle<float> (22.0f, 8.0f).withCentre ({ cx, sliderPos }), 4.0f);
    }
    else
    {
        const float cy = (float) y + (float) height / 2.0f;
        const juce::Rectangle<float> track ((float) x, cy - trackWidth / 2.0f, (float) width, trackWidth);
        g.setColour (colours::panelLight);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);
        g.setColour (colours::text);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ sliderPos, cy }));
    }
}

void WoodyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool isHighlighted, bool isDown)
{
    auto colour = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;
    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.4f);
    else if (isDown)
        colour = colour.darker (0.15f);
    else if (isHighlighted)
        colour = colour.brighter (0.12f);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (bounds, juce::jmin (bounds.getHeight() / 2.0f, 14.0f));
}

void WoodyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isHighlighted, bool)
{
    // A pill switch followed by the text
    const auto bounds = button.getLocalBounds().toFloat();
    const auto switchArea = juce::Rectangle<float> (0.0f, 0.0f, 34.0f, 18.0f).withCentre ({ 19.0f, bounds.getCentreY() });
    const bool on = button.getToggleState();

    g.setColour (on ? colours::accent : (isHighlighted ? colours::line : colours::panelLight));
    g.fillRoundedRectangle (switchArea, 9.0f);

    const float knob = 12.0f;
    const float knobX = on ? switchArea.getRight() - knob - 3.0f : switchArea.getX() + 3.0f;
    g.setColour (on ? colours::background : colours::textDim);
    g.fillEllipse (knobX, switchArea.getCentreY() - knob / 2.0f, knob, knob);

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (button.getButtonText(), bounds.withLeft (switchArea.getRight() + 8.0f), juce::Justification::centredLeft);
}

void WoodyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 7.0f);

    if (box.hasKeyboardFocus (false))
    {
        g.setColour (colours::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    }

    // Small chevron
    const float cx = (float) width - 14.0f;
    const float cy = (float) height / 2.0f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WoodyLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (juce::FontOptions (13.5f));
}

juce::Font WoodyLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.5f), juce::Font::bold);
}
} // namespace woody
