#include "Components.h"
#include "Wavetables.h"
#include <cmath>

using namespace woody;

namespace synthwoody
{
Knob::Knob (juce::AudioProcessorValueTreeState& state, ParamIndex param, const juce::String& caption, juce::Colour colour)
    : attachment (state, paramDefs()[(size_t) param].id, slider)
{
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);

    if (auto* p = state.getParameter (paramDefs()[(size_t) param].id))
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));

    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    label.setColour (juce::Label::textColourId, colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (15));
    slider.setBounds (area);
}

void paintDisplayBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (colours::background);
    g.fillRoundedRectangle (bounds, 8.0f);

    // Faint grid
    g.setColour (colours::line.withAlpha (0.45f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (bounds.getY() + bounds.getHeight() * (float) i / 4.0f), bounds.getX() + 6.0f, bounds.getRight() - 6.0f);
}

namespace
{
void strokeAndFill (juce::Graphics& g, const juce::Path& path, juce::Rectangle<float> bounds, juce::Colour colour, bool closeToBottom)
{
    if (closeToBottom)
    {
        juce::Path filled (path);
        filled.lineTo (bounds.getRight(), bounds.getBottom());
        filled.lineTo (bounds.getX(), bounds.getBottom());
        filled.closeSubPath();
        g.setColour (colour.withAlpha (0.14f));
        g.fillPath (filled);
    }

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

void AnalogWaveView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintDisplayBackground (g, bounds);

    const int shape = (int) value (aShape);
    const float pw = value (aPw);
    const auto area = bounds.reduced (10.0f, 10.0f);

    juce::Path path;
    const int points = 200;
    for (int i = 0; i <= points; ++i)
    {
        // Two cycles
        const float t = (float) i / (float) points;
        const float phase = std::fmod (t * 2.0f, 1.0f);

        float v;
        switch (shape)
        {
            case 0:  v = 2.0f * phase - 1.0f; break;
            case 1:  v = phase < pw ? 1.0f : -1.0f; break;
            case 2:  v = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            default: v = std::sin (juce::MathConstants<float>::twoPi * phase); break;
        }

        const float x = area.getX() + t * area.getWidth();
        const float y = area.getCentreY() - v * area.getHeight() * 0.5f;

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (colours::accent);
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WavetableView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintDisplayBackground (g, bounds);

    const auto& bank = WavetableBank::get();
    const int table = (int) value (bTable);
    const float position = value (bPos);
    const auto area = bounds.reduced (10.0f, 10.0f);

    // A few ghost frames behind the current one show where the position control can go
    for (int ghost = 0; ghost < numFrames; ++ghost)
    {
        const float p = (float) ghost / (float) (numFrames - 1);
        juce::Path ghostPath;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = (float) i / 120.0f;
            const float v = bank.read (table, p, 2, std::fmod (t * 2.0f, 1.0f) * 0.9999f);
            const float x = area.getX() + t * area.getWidth();
            const float y = area.getCentreY() - v * area.getHeight() * 0.5f;
            if (i == 0)
                ghostPath.startNewSubPath (x, y);
            else
                ghostPath.lineTo (x, y);
        }
        g.setColour (colours::line.withAlpha (0.55f));
        g.strokePath (ghostPath, juce::PathStrokeType (1.0f));
    }

    juce::Path path;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200.0f;
        const float v = bank.read (table, position, 2, std::fmod (t * 2.0f, 1.0f) * 0.9999f);
        const float x = area.getX() + t * area.getWidth();
        const float y = area.getCentreY() - v * area.getHeight() * 0.5f;
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (colours::accent2);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

EnvelopeView::EnvelopeView (juce::AudioProcessorValueTreeState& s, ParamIndex a, ParamIndex d, ParamIndex sus, ParamIndex r, juce::Colour c)
    : ParamView (s), pa (a), pd (d), ps (sus), pr (r), colour (c)
{
}

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintDisplayBackground (g, bounds);

    const auto area = bounds.reduced (10.0f, 12.0f);

    // Times are drawn on a compressed scale so short and long settings both stay readable
    auto width = [] (float seconds) { return 0.08f + 0.92f * std::pow (juce::jlimit (0.0f, 1.0f, seconds / 8.0f), 0.38f); };
    const float wa = width (value (pa)), wd = width (value (pd)), wr = width (value (pr));
    const float hold = 0.35f;
    const float total = wa + wd + hold + wr;
    const float sustain = value (ps);

    auto x = [&] (float units) { return area.getX() + units / total * area.getWidth(); };
    auto y = [&] (float level) { return area.getBottom() - level * area.getHeight(); };

    juce::Path path;
    path.startNewSubPath (x (0.0f), y (0.0f));
    path.lineTo (x (wa), y (1.0f));
    path.lineTo (x (wa + wd), y (sustain));
    path.lineTo (x (wa + wd + hold), y (sustain));
    path.lineTo (x (total), y (0.0f));

    strokeAndFill (g, path, area, colour, true);
}

void FilterView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintDisplayBackground (g, bounds);

    const int type = (int) value (fType);
    const float cutoffHz = value (cutoff);
    const float resonance = value (res);
    const auto area = bounds.reduced (10.0f, 12.0f);

    auto magnitude = [&] (float w) // w = frequency / cutoff
    {
        auto secondOrder = [w] (float q, bool band, bool high)
        {
            const float denom = std::sqrt ((1.0f - w * w) * (1.0f - w * w) + (w / q) * (w / q));
            const float num = band ? w / q : high ? w * w : 1.0f;
            return num / denom;
        };

        switch (type)
        {
            case 1:  return secondOrder (0.6f + resonance * 8.0f, false, false);
            case 2:  return secondOrder (0.7f + resonance * 8.0f, true, false);
            case 3:  return secondOrder (0.6f + resonance * 5.0f, false, true) * secondOrder (0.6f + resonance * 5.0f, false, true);
            default: { const float h = secondOrder (0.6f + resonance * 5.0f, false, false); return h * h; }
        }
    };

    juce::Path path;
    const int points = 160;
    for (int i = 0; i <= points; ++i)
    {
        const float t = (float) i / (float) points;
        const float freq = 20.0f * std::pow (1000.0f, t); // 20 Hz .. 20 kHz
        const float db = juce::Decibels::gainToDecibels (magnitude (freq / cutoffHz), -60.0f);

        const float x = area.getX() + t * area.getWidth();
        const float y = area.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 36.0f) / 54.0f) * area.getHeight();

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    strokeAndFill (g, path, area, colours::accent, true);

    // Cutoff marker
    const float markerX = area.getX() + std::log (cutoffHz / 20.0f) / std::log (1000.0f) * area.getWidth();
    g.setColour (colours::text.withAlpha (0.35f));
    g.drawVerticalLine ((int) markerX, area.getY(), area.getBottom());
}
} // namespace synthwoody
