#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include "WoodySaturation.h"

using namespace woody;

namespace eqwoody
{
juce::Colour bandColour (int band)
{
    return juce::Colour::fromHSV ((float) band / (float) numBands, 0.62f, 0.98f, 1.0f);
}

//==============================================================================
ResponseView::ResponseView (EqProcessor& p) : processor (p)
{
    startTimerHz (30);
}

juce::Rectangle<float> ResponseView::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (40.0f).withTrimmedRight (16.0f).withTrimmedTop (14.0f).withTrimmedBottom (28.0f);
}

float ResponseView::freqToX (float freq) const
{
    const auto plot = plotArea();
    return plot.getX() + std::log (juce::jlimit (20.0f, 20000.0f, freq) / 20.0f) / std::log (1000.0f) * plot.getWidth();
}

float ResponseView::xToFreq (float x) const
{
    const auto plot = plotArea();
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth()));
}

float ResponseView::rangeDb() const
{
    static const float ranges[] = { 6.0f, 12.0f, 24.0f, 30.0f };
    return ranges[juce::jlimit (0, 3, (int) processor.getValue ("range"))];
}

float ResponseView::gainToY (float gainDb) const
{
    const auto plot = plotArea();
    return plot.getCentreY() - gainDb / rangeDb() * plot.getHeight() * 0.5f;
}

float ResponseView::yToGain (float y) const
{
    const auto plot = plotArea();
    return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * rangeDb();
}

juce::Point<float> ResponseView::nodePosition (const BandSettings& s) const
{
    return { freqToX (s.freq), gainToY (hasGain (s.type) ? s.gain : 0.0f) };
}

int ResponseView::findNode (juce::Point<float> position) const
{
    const auto bands = processor.readBands();
    int best = -1;
    float bestDistance = 14.0f;

    for (int b = 0; b < numBands; ++b)
    {
        if (! bands[(size_t) b].on)
            continue;

        const float d = nodePosition (bands[(size_t) b]).getDistanceFrom (position);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = b;
        }
    }
    return best;
}

void ResponseView::setSelected (int band)
{
    selected = band;
    repaint();
}

void ResponseView::select (int band)
{
    selected = band;
    if (onSelect != nullptr)
        onSelect (band);
    repaint();
}

void ResponseView::setParam (const juce::String& id, float value)
{
    if (auto* p = processor.parameters.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void ResponseView::timerCallback()
{
    const int mode = (int) processor.getValue ("analyzer"); // Off, Post, Pre, Pre + Post
    if (mode == 0)
        return;

    if (mode == 2 || mode == 3)
        preSpectrum.update (processor.preFifo);
    if (mode == 1 || mode == 3)
        postSpectrum.update (processor.postFifo);

    repaint();
}

//==============================================================================
void ResponseView::paint (juce::Graphics& g)
{
    const auto plot = plotArea();
    const double sr = processor.getCurrentSampleRate();
    const float range = rangeDb();

    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 14.0f);

    g.setColour (colours::background);
    g.fillRoundedRectangle (plot.expanded (6.0f, 6.0f), 8.0f);

    // Grid and labels
    g.setFont (juce::FontOptions (11.0f));
    const float step = range <= 6.0f ? 3.0f : range <= 12.0f ? 6.0f : range <= 24.0f ? 12.0f : 10.0f;
    for (float db = -range; db <= range + 0.01f; db += step)
    {
        const float y = gainToY (db);
        g.setColour (db == 0.0f ? colours::line : colours::line.withAlpha (0.45f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::textDim);
        g.drawText ((db > 0.0f ? "+" : "") + juce::String ((int) db), (int) plot.getX() - 38, (int) y - 8, 32, 16, juce::Justification::centredRight);
    }

    static const float gridFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    static const char* gridLabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    for (int i = 0; i < 10; ++i)
    {
        const float x = freqToX (gridFreqs[i]);
        g.setColour (colours::line.withAlpha (0.45f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (colours::textDim);
        g.drawText (gridLabels[i], (int) x - 20, (int) plot.getBottom() + 6, 40, 16, juce::Justification::centred);
    }

    // Analyzer
    auto drawSpectrum = [&] (Spectrum& spectrum, juce::Colour colour, float alpha)
    {
        juce::Path path;
        bool started = false;
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
        {
            const float f = xToFreq (x);
            const float bin = f / (float) (sr * 0.5) * (float) Spectrum::numBins;
            const int b0 = (int) bin;
            const float frac = bin - (float) b0;
            float db = spectrum.dbAt (b0) * (1.0f - frac) + spectrum.dbAt (b0 + 1) * frac;
            db += 4.5f * std::log2 (f / 1000.0f); // tilt, so pink noise reads flat
            const float y = plot.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 84.0f) * plot.getHeight();

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        juce::Path filled (path);
        filled.lineTo (plot.getRight(), plot.getBottom());
        filled.lineTo (plot.getX(), plot.getBottom());
        filled.closeSubPath();
        g.setColour (colour.withAlpha (alpha * 0.35f));
        g.fillPath (filled);
        g.setColour (colour.withAlpha (alpha));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    };

    const int analyzerMode = (int) processor.getValue ("analyzer");
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plot.toNearestInt());
        if (analyzerMode == 2 || analyzerMode == 3)
            drawSpectrum (preSpectrum, colours::textDim, 0.55f);
        if (analyzerMode == 1 || analyzerMode == 3)
            drawSpectrum (postSpectrum, colours::accent2, 0.85f);
    }

    // Curves
    const auto bands = processor.readBands();
    constexpr int points = 260;
    std::array<float, points + 1> combined {};
    std::array<float, points + 1> bandCurve;

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plot.toNearestInt());

        for (int b = 0; b < numBands; ++b)
        {
            const auto& s = bands[(size_t) b];
            if (! isActive (s))
                continue;

            const auto sections = designBand (s, sr);
            for (int i = 0; i <= points; ++i)
            {
                const float f = 20.0f * std::pow (1000.0f, (float) i / (float) points);
                const double w = 2.0 * juce::MathConstants<double>::pi * (double) f / sr;
                bandCurve[(size_t) i] = (float) (20.0 * std::log10 (std::max (1.0e-6, magnitude (sections, w))));
                combined[(size_t) i] += bandCurve[(size_t) i];
            }

            if (b != selected)
                continue;

            juce::Path path;
            path.startNewSubPath (plot.getX(), gainToY (0.0f));
            for (int i = 0; i <= points; ++i)
                path.lineTo (plot.getX() + (float) i / (float) points * plot.getWidth(), gainToY (bandCurve[(size_t) i]));
            path.lineTo (plot.getRight(), gainToY (0.0f));
            path.closeSubPath();

            g.setColour (bandColour (b).withAlpha (0.22f));
            g.fillPath (path);
        }

        juce::Path total;
        for (int i = 0; i <= points; ++i)
        {
            const float x = plot.getX() + (float) i / (float) points * plot.getWidth();
            const float y = gainToY (combined[(size_t) i]);
            if (i == 0)
                total.startNewSubPath (x, y);
            else
                total.lineTo (x, y);
        }

        g.setColour (colours::text.withAlpha (0.18f));
        g.strokePath (total, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::text);
        g.strokePath (total, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Nodes
    for (int b = 0; b < numBands; ++b)
    {
        const auto& s = bands[(size_t) b];
        if (! s.on)
            continue;

        const auto p = nodePosition (s);
        const auto colour = bandColour (b);
        const bool isSelected = b == selected;

        if (isSelected || b == hovered)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillEllipse (p.x - 14.0f, p.y - 14.0f, 28.0f, 28.0f);
        }

        g.setColour (colour);
        g.fillEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f);
        g.setColour (isSelected ? colours::text : colours::background);
        g.drawEllipse (p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f, isSelected ? 2.0f : 1.5f);

        g.setColour (colours::background);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String (b + 1), (int) p.x - 8, (int) p.y - 8, 16, 16, juce::Justification::centred);
    }

    // Read-out of the band under the mouse
    const int info = dragging >= 0 ? dragging : hovered;
    if (info >= 0 && bands[(size_t) info].on)
    {
        const auto& s = bands[(size_t) info];
        const auto p = nodePosition (s);
        juce::String text = (s.freq >= 1000.0f ? juce::String (s.freq / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (s.freq)) + " Hz");
        if (hasGain (s.type))
            text << "   " << (s.gain > 0.0f ? "+" : "") << juce::String (s.gain, 1) << " dB";
        text << "   Q " << juce::String (s.q, 2);

        const float width = 12.0f + (float) text.length() * 6.6f;
        auto box = juce::Rectangle<float> (width, 22.0f).withCentre ({ p.x, p.y - 28.0f });
        box = box.withX (juce::jlimit (plot.getX(), plot.getRight() - width, box.getX()));
        if (box.getY() < plot.getY())
            box.setY (p.y + 16.0f);

        g.setColour (colours::panelLight);
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (text, box, juce::Justification::centred);
    }

    if (std::none_of (bands.begin(), bands.end(), [] (const BandSettings& s) { return s.on; }))
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (utf8 ("Clique duas vezes aqui para adicionar uma banda"), plot, juce::Justification::centred);
    }
}

//==============================================================================
void ResponseView::beginDrag (int band)
{
    dragging = band;
    gestureParams.clear();

    for (const char* name : { "freq", "gain" })
        if (auto* p = processor.parameters.getParameter (bandId (band, name)))
        {
            p->beginChangeGesture();
            gestureParams.push_back (p);
        }
}

void ResponseView::endDrag()
{
    for (auto* p : gestureParams)
        p->endChangeGesture();

    gestureParams.clear();
    dragging = -1;
}

void ResponseView::mouseDown (const juce::MouseEvent& e)
{
    const int hit = findNode (e.position);
    if (hit < 0)
        return;

    select (hit);

    if (e.mods.isPopupMenu())
    {
        showMenu (hit);
        return;
    }

    const auto bands = processor.readBands();
    dragOffset = nodePosition (bands[(size_t) hit]) - e.position;
    beginDrag (hit);
}

void ResponseView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const auto target = e.position + dragOffset;
    const auto bands = processor.readBands();
    const auto& s = bands[(size_t) dragging];

    setParam (bandId (dragging, "freq"), juce::jlimit (20.0f, 20000.0f, xToFreq (target.x)));

    if (hasGain (s.type))
    {
        float gain = juce::jlimit (-30.0f, 30.0f, yToGain (target.y));
        if (std::abs (gain) < 0.25f)
            gain = 0.0f; // sticks to 0 dB
        setParam (bandId (dragging, "gain"), gain);
    }
}

void ResponseView::mouseUp (const juce::MouseEvent&)
{
    endDrag();
}

void ResponseView::mouseMove (const juce::MouseEvent& e)
{
    const int hit = findNode (e.position);
    if (hit != hovered)
    {
        hovered = hit;
        repaint();
    }

    setMouseCursor (hit >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void ResponseView::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void ResponseView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = findNode (e.position);

    if (hit >= 0)
    {
        endDrag();
        setParam (bandId (hit, "on"), 0.0f);
        return;
    }

    if (! plotArea().expanded (6.0f).contains (e.position))
        return;

    const auto bands = processor.readBands();
    for (int b = 0; b < numBands; ++b)
    {
        if (bands[(size_t) b].on)
            continue;

        setParam (bandId (b, "type"), (float) Bell);
        setParam (bandId (b, "freq"), xToFreq (e.position.x));
        setParam (bandId (b, "gain"), juce::jlimit (-30.0f, 30.0f, yToGain (e.position.y)));
        setParam (bandId (b, "q"), 1.0f);
        setParam (bandId (b, "place"), (float) Stereo);
        setParam (bandId (b, "on"), 1.0f);
        select (b);
        return;
    }
}

void ResponseView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int hit = findNode (e.position);
    const int band = hit >= 0 ? hit : selected;
    const auto bands = processor.readBands();

    if (! bands[(size_t) band].on)
        return;

    if (auto* p = processor.parameters.getParameter (bandId (band, "q")))
    {
        const float q = bands[(size_t) band].q * std::exp (wheel.deltaY * 1.4f);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.1f, 40.0f, q)));
        p->endChangeGesture();
    }
}

void ResponseView::showMenu (int band)
{
    const auto bands = processor.readBands();
    const auto& s = bands[(size_t) band];

    juce::PopupMenu types, places;
    const auto typeList = typeNames();
    for (int i = 0; i < typeList.size(); ++i)
        types.addItem (100 + i, typeList[i], true, s.type == i);

    const auto placeList = placementNames();
    for (int i = 0; i < placeList.size(); ++i)
        places.addItem (200 + i, placeList[i], true, s.place == i);

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSubMenu ("Tipo", types);
    menu.addSubMenu (utf8 ("Posição"), places);
    menu.addSeparator();
    menu.addItem (1, "Remover banda");

    menu.showMenuAsync (juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<ResponseView> (this), band] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result == 1)
            safe->setParam (bandId (band, "on"), 0.0f);
        else if (result >= 200)
            safe->setParam (bandId (band, "place"), (float) (result - 200));
        else if (result >= 100)
            safe->setParam (bandId (band, "type"), (float) (result - 100));
    });
}

//==============================================================================
void BandChips::paint (juce::Graphics& g)
{
    const float width = (float) getWidth() / (float) numBands;

    for (int b = 0; b < numBands; ++b)
    {
        const bool on = processor.getValue (bandId (b, "on")) > 0.5f;
        const auto chip = juce::Rectangle<float> ((float) b * width, 0.0f, width, (float) getHeight()).reduced (3.0f, 2.0f);
        const auto colour = bandColour (b);

        g.setColour (on ? colour.withAlpha (0.9f) : colours::panelLight);
        g.fillRoundedRectangle (chip, chip.getHeight() / 2.0f);

        if (b == selected)
        {
            g.setColour (colours::text);
            g.drawRoundedRectangle (chip.reduced (0.5f), chip.getHeight() / 2.0f, 2.0f);
        }

        g.setColour (on ? colours::background : colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (b + 1), chip, juce::Justification::centred);
    }
}

void BandChips::mouseDown (const juce::MouseEvent& e)
{
    const int band = juce::jlimit (0, numBands - 1, (int) ((float) e.x / ((float) getWidth() / (float) numBands)));
    selected = band;
    if (onSelect != nullptr)
        onSelect (band);
    repaint();
}

//==============================================================================
BandPanel::BandPanel (EqProcessor& p) : processor (p)
{
    auto fill = [] (juce::ComboBox& box, const juce::StringArray& items)
    {
        for (int i = 0; i < items.size(); ++i)
            box.addItem (items[i], i + 1);
    };

    fill (typeBox, typeNames());
    fill (slopeBox, slopeNames());
    fill (placeBox, placementNames());
    fill (satModeBox, juce::StringArray::fromTokens (woody::satModeChoices, "|", ""));

    for (auto* box : { &typeBox, &slopeBox, &placeBox, &satModeBox })
        addAndMakeVisible (*box);

    onButton.setButtonText ("Banda ativa");
    addAndMakeVisible (onButton);

    titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (titleLabel);

    auto caption = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (l);
    };
    caption (typeLabel, "TIPO");
    caption (slopeLabel, utf8 ("INCLINAÇÃO"));
    caption (placeLabel, utf8 ("POSIÇÃO NO ESTÉREO"));
    caption (satModeLabel, utf8 ("CURVA DA SATURAÇÃO"));

    setBand (0);
}

void BandPanel::setBand (int newBand)
{
    band = newBand;

    // Detach before re-attaching to the new band's parameters
    onAttachment.reset();
    typeAttachment.reset();
    slopeAttachment.reset();
    placeAttachment.reset();
    satModeAttachment.reset();

    auto& state = processor.parameters;
    const auto colour = bandColour (band);

    onAttachment = std::make_unique<ButtonAttachment> (state, bandId (band, "on"), onButton);
    typeAttachment = std::make_unique<ComboAttachment> (state, bandId (band, "type"), typeBox);
    slopeAttachment = std::make_unique<ComboAttachment> (state, bandId (band, "slope"), slopeBox);
    placeAttachment = std::make_unique<ComboAttachment> (state, bandId (band, "place"), placeBox);
    satModeAttachment = std::make_unique<ComboAttachment> (state, bandId (band, "satmode"), satModeBox);

    freq = std::make_unique<ParamKnob> (state, bandId (band, "freq"), utf8 ("Frequência"), colour);
    gain = std::make_unique<ParamKnob> (state, bandId (band, "gain"), "Ganho", colour);
    q = std::make_unique<ParamKnob> (state, bandId (band, "q"), "Q", colour);
    sat = std::make_unique<ParamKnob> (state, bandId (band, "sat"), utf8 ("Saturação"), colour);

    for (auto* k : { freq.get(), gain.get(), q.get(), sat.get() })
        addAndMakeVisible (*k);

    titleLabel.setText ("BANDA " + juce::String (band + 1), juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, colour);

    resized();
    refreshState();
    repaint();
}

void BandPanel::refreshState()
{
    const auto bands = processor.readBands();
    const auto& s = bands[(size_t) band];

    slopeBox.setEnabled (isCut (s.type));
    gain->setEnabled (hasGain (s.type));
    gain->setAlpha (hasGain (s.type) ? 1.0f : 0.35f);
    slopeBox.setAlpha (isCut (s.type) ? 1.0f : 0.4f);
    q->setCaption (isCut (s.type) ? utf8 ("Ressonância") : juce::String ("Q"));
}

void BandPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 14.0f);

    g.setColour (bandColour (band));
    g.fillRoundedRectangle (16.0f, 18.0f, 4.0f, 18.0f, 2.0f);
}

void BandPanel::resized()
{
    if (freq == nullptr)
        return;

    titleLabel.setBounds (30, 14, 150, 26);
    onButton.setBounds (190, 14, 130, 26);

    const int knobY = 56;
    freq->setBounds (24, knobY, 110, 130);
    gain->setBounds (140, knobY, 110, 130);
    q->setBounds (256, knobY, 110, 130);
    sat->setBounds (400, knobY, 110, 130);

    auto place = [] (juce::Label& label, juce::ComboBox& box, int x, int y, int w)
    {
        label.setBounds (x, y, w, 16);
        box.setBounds (x, y + 18, w, 30);
    };
    place (typeLabel, typeBox, 560, 52, 230);
    place (slopeLabel, slopeBox, 810, 52, 230);
    place (placeLabel, placeBox, 560, 122, 230);
    place (satModeLabel, satModeBox, 810, 122, 230);
}
} // namespace eqwoody

//==============================================================================
EqView::EqView (EqProcessor& p)
    : processor (p),
      outBar (p.parameters, "out", utf8 ("Saída"), colours::accent),
      response (p), chips (p), panel (p)
{
    title.setText ("EQ WOODYS", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    latencyLabel.setFont (juce::FontOptions (12.0f));
    latencyLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (latencyLabel);

    auto fill = [] (juce::ComboBox& box, const juce::StringArray& items)
    {
        for (int i = 0; i < items.size(); ++i)
            box.addItem (items[i], i + 1);
    };

    fill (phaseBox, { "Zero Latency", "Linear Phase" });
    fill (qualityBox, eqwoody::qualityNames());
    fill (analyzerBox, { "Analisador: off", utf8 ("Pós-EQ"), utf8 ("Pré-EQ"), utf8 ("Pré + Pós") });
    fill (rangeBox, { utf8 ("±6 dB"), utf8 ("±12 dB"), utf8 ("±24 dB"), utf8 ("±30 dB") });

    phaseBox.setTooltip (utf8 ("Zero Latency: filtros IIR, sem atraso, com a fase de um EQ analógico.\n"
                               "Linear Phase: filtro FIR simétrico, sem distorção de fase, com latência."));
    qualityBox.setTooltip (utf8 ("Tamanho do filtro de fase linear. Maior = graves mais precisos e mais latência."));

    phaseAttachment = std::make_unique<ComboAttachment> (p.parameters, "phase", phaseBox);
    qualityAttachment = std::make_unique<ComboAttachment> (p.parameters, "quality", qualityBox);
    analyzerAttachment = std::make_unique<ComboAttachment> (p.parameters, "analyzer", analyzerBox);
    rangeAttachment = std::make_unique<ComboAttachment> (p.parameters, "range", rangeBox);

    for (auto* box : { &phaseBox, &qualityBox, &analyzerBox, &rangeBox })
        addAndMakeVisible (*box);

    bypassButton.setButtonText ("Bypass");
    bypassAttachment = std::make_unique<ButtonAttachment> (p.parameters, "bypass", bypassButton);
    addAndMakeVisible (bypassButton);

    resetButton.setTooltip (utf8 ("Desliga todas as bandas e volta tudo ao padrão"));
    resetButton.onClick = [this] { processor.resetAll(); };
    addAndMakeVisible (resetButton);

    addAndMakeVisible (outBar);
    addAndMakeVisible (outMeter);

    response.onSelect = [this] (int band) { select (band); };
    chips.onSelect = [this] (int band) { select (band); };
    addAndMakeVisible (response);
    addAndMakeVisible (chips);
    addAndMakeVisible (panel);

    startTimerHz (20);
}

void EqView::select (int band)
{
    response.setSelected (band);
    chips.setSelected (band);
    panel.setBand (band);
}

void EqView::timerCallback()
{
    chips.repaint();
    panel.refreshState();
    outMeter.setLevel (processor.outputPeak.load());

    const bool linear = processor.getValue ("phase") > 0.5f;
    qualityBox.setEnabled (linear);
    qualityBox.setAlpha (linear ? 1.0f : 0.4f);

    const int latency = processor.getLatencySamples();
    latencyLabel.setText (linear ? utf8 ("Latência: ") + juce::String (latency * 1000.0 / processor.getCurrentSampleRate(), 1) + " ms ("
                                       + juce::String (latency) + " amostras)"
                                 : utf8 ("Latência: zero"),
                          juce::dontSendNotification);
}

void EqView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Logo mark: three offset circles
    const float cx = 30.0f, cy = 32.0f;
    g.setColour (colours::accent);
    g.fillEllipse (cx - 13.0f, cy - 13.0f, 18.0f, 18.0f);
    g.setColour (colours::accent2.withAlpha (0.85f));
    g.fillEllipse (cx - 3.0f, cy - 7.0f, 14.0f, 14.0f);
    g.setColour (colours::text.withAlpha (0.8f));
    g.fillEllipse (cx - 9.0f, cy + 3.0f, 8.0f, 8.0f);
}

void EqView::resized()
{
    title.setBounds (50, 10, 210, 30);
    phaseBox.setBounds (270, 8, 150, 30);
    qualityBox.setBounds (428, 8, 100, 30);
    analyzerBox.setBounds (536, 8, 118, 30);
    rangeBox.setBounds (662, 8, 84, 30);
    outBar.setBounds (762, 8, 226, 30);
    outMeter.setBounds (762, 42, 226, 6);
    bypassButton.setBounds (1004, 10, 86, 26);
    resetButton.setBounds (1000, 38, 90, 20);
    latencyLabel.setBounds (270, 42, 380, 16);

    response.setBounds (12, 66, 1076, 360);
    chips.setBounds (12, 434, 1076, 34);
    panel.setBounds (12, 476, 1076, 212);
}

//==============================================================================
EqEditor::EqEditor (EqProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    // Read the saved size first: setting the limits below resizes the (still empty) window
    const int width = juce::jlimit (770, 1980, processor.editorWidth > 0 ? processor.editorWidth : EqView::designWidth);

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    view.setBounds (0, 0, EqView::designWidth, EqView::designHeight);

    setResizable (true, true);
    setResizeLimits (770, 490, 1980, 1260);
    if (auto* sizeLimits = getConstrainer())
        sizeLimits->setFixedAspectRatio ((double) EqView::designWidth / EqView::designHeight);

    setSize (width, width * EqView::designHeight / EqView::designWidth);
    sized = true;
}

EqEditor::~EqEditor()
{
    setLookAndFeel (nullptr);
}

void EqEditor::resized()
{
    const float scale = (float) getWidth() / (float) EqView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sized)
        processor.editorWidth = getWidth();
}
