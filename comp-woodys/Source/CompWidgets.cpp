#include "CompWidgets.h"

using namespace woody;

namespace compwoody
{
namespace
{
constexpr float minDisplayDb = -60.0f;
constexpr float maxFreqShown = 20000.0f;
constexpr float minFreqShown = 20.0f;

juce::String freqLabel (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}
} // namespace

juce::Colour bandColour (int band)
{
    static const juce::Colour colours[numBands] { juce::Colour (0xff3dd6c6), juce::Colour (0xff5b8cff),
                                                  juce::Colour (0xffb26bff), juce::Colour (0xffff8a3d) };
    return colours[juce::jlimit (0, numBands - 1, band)];
}

juce::String bandName (int band)
{
    static const char* names[numBands] { "Graves", "Médios-graves", "Médios-agudos", "Agudos" };
    return utf8 (names[juce::jlimit (0, numBands - 1, band)]);
}

//==============================================================================
void GrBar::setReduction (float db)
{
    level = db > level ? db : level * 0.9f;
    if (level < 0.01f)
        level = 0.0f;
    repaint();
}

void GrBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (bounds, 3.0f);

    const float proportion = juce::jlimit (0.0f, 1.0f, level / rangeDb);
    if (proportion > 0.0f)
    {
        g.setColour (colours::accent);
        g.fillRoundedRectangle (bounds.withHeight (juce::jmax (3.0f, bounds.getHeight() * proportion)), 3.0f);
    }

    g.setColour (colours::background.withAlpha (0.55f));
    for (float db = 6.0f; db < rangeDb; db += 6.0f)
        g.fillRect (bounds.getX(), bounds.getY() + bounds.getHeight() * db / rangeDb, bounds.getWidth(), 1.0f);
}

//==============================================================================
TransferCurve::TransferCurve (CompProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void TransferCurve::timerCallback()
{
    const float target = juce::jmax (minDisplayDb - 5.0f, processor.getEngine().getInputLevelDb());
    inputDb = target > inputDb ? target : inputDb + (target - inputDb) * 0.2f;
    repaint();
}

void TransferCurve::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto plot = bounds.reduced (36.0f, 30.0f).withTrimmedTop (4.0f).withTrimmedBottom (6.0f);
    auto toX = [&] (float db) { return plot.getX() + (db - minDisplayDb) / -minDisplayDb * plot.getWidth(); };
    auto toY = [&] (float db) { return plot.getBottom() - (db - minDisplayDb) / -minDisplayDb * plot.getHeight(); };

    g.setFont (juce::FontOptions (10.5f));
    for (float db = minDisplayDb; db <= 0.01f; db += 12.0f)
    {
        g.setColour (colours::line.withAlpha (0.7f));
        g.fillRect (toX (db), plot.getY(), 1.0f, plot.getHeight());
        g.fillRect (plot.getX(), toY (db), plot.getWidth(), 1.0f);

        g.setColour (colours::textDim);
        g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (toX (db) - 18.0f, plot.getBottom() + 4.0f, 36.0f, 14.0f), juce::Justification::centred);
        g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (plot.getX() - 34.0f, toY (db) - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
    }

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (utf8 ("ENTRADA (dB)"), juce::Rectangle<float> (plot.getX(), bounds.getBottom() - 16.0f, plot.getWidth(), 14.0f), juce::Justification::centred);
    g.drawText (utf8 ("SAÍDA"), juce::Rectangle<float> (6.0f, 8.0f, 60.0f, 14.0f), juce::Justification::centredLeft);

    const float threshold = processor.getValue ("threshold");
    const float ratio = processor.getValue ("ratio");
    const float knee = processor.getValue ("knee");
    const float inv = inverseRatio (ratio);
    const float makeup = processor.getValue ("automakeup") > 0.5f ? autoMakeupDb (threshold, inv) : processor.getValue ("makeup");

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (plot.toNearestInt().expanded (1));

    // The knee region
    if (knee > 0.1f)
    {
        g.setColour (colours::accent.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (toX (threshold - knee * 0.5f), plot.getY(), toX (threshold + knee * 0.5f) - toX (threshold - knee * 0.5f), plot.getHeight()));
    }

    // Unity line
    {
        juce::Path unity;
        unity.startNewSubPath (toX (minDisplayDb), toY (minDisplayDb));
        unity.lineTo (toX (0.0f), toY (0.0f));
        juce::Path dashed;
        const float dashes[] { 4.0f, 5.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dashes, 2);
        g.setColour (colours::textDim.withAlpha (0.5f));
        g.fillPath (dashed);
    }

    auto outputFor = [&] (float db) { return db - gainReductionDb (db, threshold, inv, knee) + makeup; };

    juce::Path curve;
    for (float db = minDisplayDb; db <= 0.01f; db += 0.5f)
    {
        const juce::Point<float> point (toX (db), toY (outputFor (db)));
        if (db <= minDisplayDb)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    juce::Path fill (curve);
    fill.lineTo (toX (0.0f), plot.getBottom());
    fill.lineTo (toX (minDisplayDb), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.22f), 0.0f, plot.getY(), colours::accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);

    g.setColour (colours::accent);
    g.strokePath (curve, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Threshold marker
    g.setColour (colours::text.withAlpha (0.55f));
    g.fillRect (toX (threshold) - 0.5f, plot.getY(), 1.0f, plot.getHeight());
    juce::Path marker;
    marker.addTriangle (toX (threshold) - 5.0f, plot.getBottom() + 0.0f, toX (threshold) + 5.0f, plot.getBottom(), toX (threshold), plot.getBottom() - 7.0f);
    g.setColour (colours::text);
    g.fillPath (marker);

    if (inputDb > minDisplayDb)
    {
        const float level = juce::jmin (0.0f, inputDb);
        const juce::Point<float> dot (toX (level), toY (outputFor (level)));

        g.setColour (colours::accent2.withAlpha (0.35f));
        g.fillRect (dot.x - 0.5f, dot.y, 1.0f, plot.getBottom() - dot.y);
        g.setColour (colours::accent2.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (dot));
        g.setColour (colours::text);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (dot));
        g.setColour (colours::accent2);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (dot));
    }
}

//==============================================================================
HistoryView::HistoryView (CompProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void HistoryView::timerCallback()
{
    history[(size_t) position] = processor.getEngine().getSingleGainReductionDb();
    position = (position + 1) % numPoints;
    repaint();
}

void HistoryView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto plot = bounds.reduced (36.0f, 30.0f).withTrimmedTop (4.0f).withTrimmedBottom (6.0f);
    auto toY = [&] (float db) { return plot.getY() + db / GrBar::rangeDb * plot.getHeight(); };

    g.setFont (juce::FontOptions (10.5f));
    for (float db = 0.0f; db <= GrBar::rangeDb + 0.01f; db += 6.0f)
    {
        g.setColour (colours::line.withAlpha (0.7f));
        g.fillRect (plot.getX(), toY (db), plot.getWidth(), 1.0f);
        g.setColour (colours::textDim);
        g.drawText (db < 0.1f ? "0" : "-" + juce::String ((int) db), juce::Rectangle<float> (plot.getX() - 34.0f, toY (db) - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
    }

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (utf8 ("REDUÇÃO DE GANHO (dB)"), juce::Rectangle<float> (6.0f, 8.0f, 240.0f, 14.0f), juce::Justification::centredLeft);

    juce::Path line;
    for (int i = 0; i < numPoints; ++i)
    {
        const float db = juce::jmin (GrBar::rangeDb, history[(size_t) ((position + i) % numPoints)]);
        const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) (numPoints - 1), toY (db));
        if (i == 0)
            line.startNewSubPath (point);
        else
            line.lineTo (point);
    }

    juce::Path fill (line);
    fill.lineTo (plot.getRight(), plot.getY());
    fill.lineTo (plot.getX(), plot.getY());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.45f), 0.0f, plot.getY(), colours::accent.withAlpha (0.08f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);

    g.setColour (colours::accent);
    g.strokePath (line, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
ModeSwitch::ModeSwitch (juce::AudioProcessorValueTreeState& state)
    : attachment (*state.getParameter ("mode"),
                  [this] (float value)
                  {
                      multiband = value > 0.5f;
                      repaint();
                      if (onChange != nullptr)
                          onChange (multiband);
                  })
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModeSwitch::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (bounds, bounds.getHeight() / 2.0f);

    const auto half = bounds.getWidth() / 2.0f;
    const auto active = bounds.withWidth (half).translated (multiband ? half : 0.0f, 0.0f).reduced (3.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (active, active.getHeight() / 2.0f);

    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.setColour (multiband ? colours::textDim : colours::background);
    g.drawText (utf8 ("Banda única"), bounds.withWidth (half), juce::Justification::centred);
    g.setColour (multiband ? colours::background : colours::textDim);
    g.drawText ("Multibanda", bounds.withTrimmedLeft (half), juce::Justification::centred);
}

void ModeSwitch::mouseDown (const juce::MouseEvent& e)
{
    const bool wantMultiband = e.position.x >= (float) getWidth() / 2.0f;
    if (wantMultiband != multiband)
        attachment.setValueAsCompleteGesture (wantMultiband ? 1.0f : 0.0f);
}

//==============================================================================
FreqStrip::FreqStrip (CompProcessor& p) : processor (p)
{
    startTimerHz (30);
}

juce::Rectangle<float> FreqStrip::plotArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 0.0f).withTrimmedTop (12.0f).withTrimmedBottom (26.0f);
}

float FreqStrip::freqToX (float freq) const
{
    const auto plot = plotArea();
    const float proportion = std::log (freq / minFreqShown) / std::log (maxFreqShown / minFreqShown);
    return plot.getX() + juce::jlimit (0.0f, 1.0f, proportion) * plot.getWidth();
}

float FreqStrip::xToFreq (float x) const
{
    const auto plot = plotArea();
    const float proportion = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return minFreqShown * std::pow (maxFreqShown / minFreqShown, proportion);
}

std::array<float, numCrossovers> FreqStrip::crossovers() const
{
    return orderedCrossovers ({ processor.getValue ("x1"), processor.getValue ("x2"), processor.getValue ("x3") },
                              processor.getCurrentSampleRate());
}

int FreqStrip::findHandle (float x) const
{
    const auto freqs = crossovers();
    int best = -1;
    float bestDistance = 16.0f;

    for (int i = 0; i < numCrossovers; ++i)
    {
        const float distance = std::abs (freqToX (freqs[(size_t) i]) - x);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

juce::RangedAudioParameter* FreqStrip::parameter (int handle) const
{
    return processor.parameters.getParameter ("x" + juce::String (handle + 1));
}

void FreqStrip::timerCallback()
{
    for (int b = 0; b < numBands; ++b)
    {
        const float target = processor.getEngine().getBandGainReductionDb (b);
        auto& shown = shownReduction[(size_t) b];
        shown = target > shown ? target : shown * 0.88f;
    }
    repaint();
}

void FreqStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto plot = plotArea();
    const auto freqs = crossovers();

    bool anySolo = false;
    for (int b = 0; b < numBands; ++b)
        anySolo = anySolo || processor.getValue (bandId (b, "solo")) > 0.5f;

    for (int b = 0; b < numBands; ++b)
    {
        const float left = b == 0 ? plot.getX() : freqToX (freqs[(size_t) b - 1]);
        const float right = b == numBands - 1 ? plot.getRight() : freqToX (freqs[(size_t) b]);
        const auto region = juce::Rectangle<float> (left, plot.getY(), right - left, plot.getHeight());
        const bool muted = anySolo && processor.getValue (bandId (b, "solo")) < 0.5f;
        const bool bypassed = processor.getValue (bandId (b, "bypass")) > 0.5f;
        const auto colour = bandColour (b).withMultipliedAlpha (muted ? 0.35f : 1.0f);

        g.setColour (colour.withAlpha (0.14f));
        g.fillRect (region);

        const float reduction = bypassed ? 0.0f : juce::jmin (shownReduction[(size_t) b], 24.0f);
        if (reduction > 0.05f)
        {
            const float height = region.getHeight() * reduction / 24.0f;
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.65f), 0.0f, region.getY(), colour.withAlpha (0.12f), 0.0f, region.getY() + height, false));
            g.fillRect (region.withHeight (height));
            g.setColour (colour);
            g.fillRect (region.getX(), region.getY() + height - 1.0f, region.getWidth(), 2.0f);
        }

        g.setColour (colour.brighter (0.2f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (bandName (b), region.withHeight (30.0f).translated (0.0f, 8.0f), juce::Justification::centred);

        if (reduction > 0.05f)
        {
            g.setColour (colours::text.withAlpha (muted ? 0.4f : 0.9f));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("-" + juce::String (reduction, 1) + " dB", region.withHeight (20.0f).translated (0.0f, 34.0f), juce::Justification::centred);
        }
    }

    // Frequency grid
    g.setFont (juce::FontOptions (10.5f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = freqToX (f);
        g.setColour (colours::line.withAlpha (0.55f));
        g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (colours::textDim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 6.0f, 40.0f, 14.0f), juce::Justification::centred);
    }

    // Crossover handles
    for (int i = 0; i < numCrossovers; ++i)
    {
        const float x = freqToX (freqs[(size_t) i]);
        const bool active = i == hovered || i == dragging;
        const float y = plot.getBottom() - 46.0f;

        g.setColour (colours::text.withAlpha (active ? 0.9f : 0.5f));
        g.fillRect (x - 0.75f, plot.getY(), 1.5f, plot.getHeight());

        const auto pill = juce::Rectangle<float> (64.0f, 24.0f).withCentre ({ x, y });
        g.setColour (active ? colours::text : colours::panelLight);
        g.fillRoundedRectangle (pill, 12.0f);
        g.setColour (active ? colours::background : colours::text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (freqLabel (freqs[(size_t) i]), pill, juce::Justification::centred);
    }
}

void FreqStrip::mouseMove (const juce::MouseEvent& e)
{
    const int handle = findHandle (e.position.x);
    if (handle != hovered)
    {
        hovered = handle;
        setMouseCursor (handle >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
}

void FreqStrip::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
}

void FreqStrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = findHandle (e.position.x);
    if (dragging >= 0)
        parameter (dragging)->beginChangeGesture();
}

void FreqStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const auto freqs = crossovers();
    const float lowest = dragging == 0 ? minCrossoverFreq : freqs[(size_t) dragging - 1] * minCrossoverSpacing;
    const float highest = dragging == numCrossovers - 1 ? juce::jmin (maxFreqShown, (float) (processor.getCurrentSampleRate() * 0.45))
                                                         : freqs[(size_t) dragging + 1] / minCrossoverSpacing;

    auto* p = parameter (dragging);
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (lowest, juce::jmax (lowest, highest), xToFreq (e.position.x))));
}

void FreqStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        parameter (dragging)->endChangeGesture();
    dragging = -1;
}

void FreqStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int handle = findHandle (e.position.x);
    if (handle < 0)
        return;

    auto* p = parameter (handle);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
}
} // namespace compwoody
