#include "PluginEditor.h"
#include "WoodySaturation.h"

using namespace woody;

namespace satwoody
{
namespace
{
constexpr float curveRangeX = 2.0f;
constexpr float curveRangeY = 1.4f;
constexpr float biasOffset = 0.5f;   // must match the engine
}

CurveView::CurveView (SatProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void CurveView::timerCallback()
{
    for (size_t i = trail.size() - 1; i > 0; --i)
        trail[i] = trail[i - 1];

    const float offset = processor.getValue ("bias") * biasOffset;
    trail[0] = processor.engine.curveInput.load() - offset;
    repaint();
}

void CurveView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto area = bounds.reduced (14.0f);
    auto toX = [&] (float x) { return area.getX() + (x / curveRangeX * 0.5f + 0.5f) * area.getWidth(); };
    auto toY = [&] (float y) { return area.getCentreY() - y / curveRangeY * 0.5f * area.getHeight(); };

    g.setColour (colours::line);
    g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);
    g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom(), 1.0f);
    for (float edge : { -1.0f, 1.0f })
    {
        g.setColour (colours::line.withAlpha (0.5f));
        g.drawLine (area.getX(), toY (edge), area.getRight(), toY (edge), 1.0f);
        g.drawLine (toX (edge), area.getY(), toX (edge), area.getBottom(), 1.0f);
    }

    // Straight line for reference
    g.setColour (colours::textDim.withAlpha (0.45f));
    g.drawLine (toX (-curveRangeY), toY (-curveRangeY), toX (curveRangeY), toY (curveRangeY), 1.0f);

    const auto mode = (SatMode) juce::jlimit (0, 4, (int) processor.getValue ("type"));
    const float offset = processor.getValue ("bias") * biasOffset;

    juce::Path path;
    const int steps = (int) area.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = ((float) i / (float) steps * 2.0f - 1.0f) * curveRangeX;
        const float y = (float) sat::shape (mode, (double) (x + offset));
        const juce::Point<float> point { toX (x), toY (juce::jlimit (-curveRangeY, curveRangeY, y)) };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    g.setColour (colours::accent);
    g.strokePath (path, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (size_t i = trail.size(); i-- > 0;)
    {
        const float x = juce::jlimit (-curveRangeX, curveRangeX, trail[i]);
        const float y = (float) sat::shape (mode, (double) (x + offset));
        const float size = i == 0 ? 10.0f : 7.0f;
        g.setColour (colours::accent2.withAlpha (i == 0 ? 1.0f : 0.35f / (float) i + 0.05f));
        g.fillEllipse (toX (x) - size * 0.5f, toY (juce::jlimit (-curveRangeY, curveRangeY, y)) - size * 0.5f, size, size);
    }
    g.restoreState();

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (utf8 ("CURVA"), getLocalBounds().reduced (16, 8).removeFromTop (14), juce::Justification::topLeft);
    g.drawText (utf8 ("entrada"), getLocalBounds().reduced (16, 8).removeFromBottom (14), juce::Justification::bottomRight);
}

//==============================================================================
ScopeView::ScopeView (SatProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void ScopeView::timerCallback()
{
    const auto& engine = processor.engine;
    const int start = engine.scopePosition.load (std::memory_order_acquire);
    const int size = SatEngine::scopeSize;

    // Start the picture at a rising zero crossing so a steady tone stands still
    int trigger = 0;
    for (int k = 1; k < size - shown; ++k)
    {
        const float before = engine.scopeDry[(size_t) ((start + k - 1) % size)].load (std::memory_order_relaxed);
        const float after = engine.scopeDry[(size_t) ((start + k) % size)].load (std::memory_order_relaxed);
        if (before < 0.0f && after >= 0.0f)
        {
            trigger = k;
            break;
        }
    }

    for (int i = 0; i < shown; ++i)
    {
        const auto index = (size_t) ((start + trigger + i) % size);
        dry[(size_t) i] = engine.scopeDry[index].load (std::memory_order_relaxed);
        wet[(size_t) i] = engine.scopeWet[index].load (std::memory_order_relaxed);
    }

    repaint();
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto area = bounds.reduced (14.0f);
    g.setColour (colours::line);
    g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);

    auto toY = [&] (float v) { return area.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * 0.5f * area.getHeight(); };
    auto draw = [&] (const std::array<float, shown>& data, juce::Colour colour, float thickness)
    {
        juce::Path path;
        for (int i = 0; i < shown; ++i)
        {
            const juce::Point<float> point { area.getX() + (float) i / (float) (shown - 1) * area.getWidth(), toY (data[(size_t) i]) };
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
    };

    draw (dry, colours::textDim.withAlpha (0.8f), 1.5f);
    draw (wet, colours::accent, 2.2f);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (colours::textDim);
    g.drawText (utf8 ("ONDA"), getLocalBounds().reduced (16, 8).removeFromTop (14), juce::Justification::topLeft);

    auto legend = getLocalBounds().reduced (16, 8).removeFromTop (14);
    g.setColour (colours::accent);
    g.drawText (utf8 ("saturado"), legend.removeFromRight (60), juce::Justification::centredRight);
    g.setColour (colours::textDim);
    g.drawText (utf8 ("original"), legend.removeFromRight (64), juce::Justification::centredRight);
}
} // namespace satwoody

//==============================================================================
namespace
{
const juce::Rectangle<int> panelOne { 20, 376, 450, 208 };
const juce::Rectangle<int> panelTwo { 482, 376, 230, 208 };
const juce::Rectangle<int> panelThree { 724, 376, 216, 208 };
}

SatView::SatView (SatProcessor& p)
    : processor (p),
      input (p.parameters, "in", "Entrada", colours::accent2),
      drive (p.parameters, "drive", "Drive", colours::accent),
      tone (p.parameters, "tone", "Tom", colours::accent),
      bias (p.parameters, "bias", "Assimetria", colours::accent),
      focusLow (p.parameters, "focuslow", "Corte grave", colours::accent2),
      focusHigh (p.parameters, "focushigh", "Corte agudo", colours::accent2),
      mix (p.parameters, "mix", "Mix", colours::accent),
      output (p.parameters, "out", utf8 ("Saída"), colours::accent2),
      curve (p),
      scope (p)
{
    title.setText ("SAT WOODYS", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    latencyLabel.setFont (juce::FontOptions (12.0f));
    latencyLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (latencyLabel);

    setupCaption (typeCaption, "Tipo de curva");
    setupCaption (stereoCaption, "Canais saturados");
    setupCaption (inCaption, "ENT");
    setupCaption (outCaption, utf8 ("SAÍ"));
    inCaption.setJustificationType (juce::Justification::centred);
    outCaption.setJustificationType (juce::Justification::centred);

    auto fill = [] (juce::ComboBox& box, const juce::StringArray& items)
    {
        for (int i = 0; i < items.size(); ++i)
            box.addItem (items[i], i + 1);
    };

    fill (typeBox, satwoody::typeNames());
    fill (stereoBox, satwoody::stereoNames());
    fill (osBox, { utf8 ("Sobreamostragem: Off"), utf8 ("Sobreamostragem: 2x"),
                   utf8 ("Sobreamostragem: 4x"), utf8 ("Sobreamostragem: 8x") });

    typeBox.setTooltip (utf8 ("Suave: curva simétrica, só harmônicos ímpares.\nVálvula: assimétrica, adiciona harmônicos pares.\n"
                              "Fita: compressão macia.\nDuro: corte seco.\nDobra: o sinal dobra de volta ao passar do limite."));
    stereoBox.setTooltip (utf8 ("Estéreo: satura esquerda e direita.\nMid: satura só o centro.\nSide: satura só as laterais."));
    osBox.setTooltip (utf8 ("Reduz o aliasing da saturação. Mais alto = mais limpo e mais latência (compensada no mix)."));
    drive.slider.setTooltip (utf8 ("Quanto o sinal é empurrado para dentro da curva."));
    bias.slider.setTooltip (utf8 ("Desloca a curva para um lado: adiciona harmônicos pares."));
    tone.slider.setTooltip (utf8 ("Inclina o brilho do sinal saturado: para a esquerda escurece, para a direita clareia."));
    focusLow.slider.setTooltip (utf8 ("Abaixo desta frequência o som passa limpo, sem saturar."));
    focusHigh.slider.setTooltip (utf8 ("Acima desta frequência o som passa limpo, sem saturar."));
    autoButton.setTooltip (utf8 ("Mantém o volume parecido ao mexer no drive."));

    typeAttachment = std::make_unique<ComboAttachment> (p.parameters, "type", typeBox);
    stereoAttachment = std::make_unique<ComboAttachment> (p.parameters, "stereo", stereoBox);
    osAttachment = std::make_unique<ComboAttachment> (p.parameters, "os", osBox);
    bypassAttachment = std::make_unique<ButtonAttachment> (p.parameters, "bypass", bypassButton);
    autoAttachment = std::make_unique<ButtonAttachment> (p.parameters, "auto", autoButton);

    for (auto* c : std::initializer_list<juce::Component*> { &typeBox, &stereoBox, &osBox, &bypassButton, &autoButton,
                                                              &input, &drive, &tone, &bias, &focusLow, &focusHigh,
                                                              &mix, &output, &inMeter, &outMeter, &curve, &scope })
        addAndMakeVisible (c);

    startTimerHz (30);
    timerCallback();
}

void SatView::setupCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (label);
}

void SatView::timerCallback()
{
    inMeter.setLevel (processor.engine.inputPeak.load());
    outMeter.setLevel (processor.engine.outputPeak.load());

    const int latency = processor.getLatency();
    if (latency != shownLatency)
    {
        shownLatency = latency;
        latencyLabel.setText (utf8 ("Latência: ") + juce::String (latency) + " amostras ("
                                  + juce::String (1000.0 * latency / juce::jmax (1.0, processor.getSampleRate()), 1) + " ms)",
                              juce::dontSendNotification);
    }
}

void SatView::paint (juce::Graphics& g)
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

    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    for (const auto& [area, name] : { std::pair { panelOne, utf8 ("SATURAÇÃO") },
                                      std::pair { panelTwo, utf8 ("FOCO") },
                                      std::pair { panelThree, utf8 ("SAÍDA") } })
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (colours::textDim);
        g.drawText (name, area.getX() + 16, area.getY() + 10, 200, 16, juce::Justification::centredLeft);
    }
}

void SatView::resized()
{
    title.setBounds (50, 10, 200, 30);
    latencyLabel.setBounds (270, 16, 270, 20);
    osBox.setBounds (560, 12, 220, 30);
    bypassButton.setBounds (830, 14, 110, 26);

    inMeter.setBounds (20, 64, 12, 286);
    curve.setBounds (40, 64, 290, 286);
    scope.setBounds (344, 64, 576, 286);
    outMeter.setBounds (928, 64, 12, 286);
    inCaption.setBounds (14, 352, 24, 14);
    outCaption.setBounds (922, 352, 24, 14);

    const int knobWidth = 96, knobHeight = 124;

    {
        const auto p = panelOne;
        typeCaption.setBounds (p.getX() + 16, p.getY() + 30, 200, 14);
        typeBox.setBounds (p.getX() + 16, p.getY() + 46, 200, 26);
        int x = p.getX() + 16;
        for (auto* k : { &input, &drive, &tone, &bias })
        {
            k->setBounds (x, p.getY() + 78, knobWidth, knobHeight);
            x += 108;
        }
    }
    {
        const auto p = panelTwo;
        stereoCaption.setBounds (p.getX() + 16, p.getY() + 30, 200, 14);
        stereoBox.setBounds (p.getX() + 16, p.getY() + 46, 198, 26);
        focusLow.setBounds (p.getX() + 16, p.getY() + 78, knobWidth, knobHeight);
        focusHigh.setBounds (p.getX() + 118, p.getY() + 78, knobWidth, knobHeight);
    }
    {
        const auto p = panelThree;
        autoButton.setBounds (p.getX() + 16, p.getY() + 46, 170, 26);
        mix.setBounds (p.getX() + 12, p.getY() + 78, knobWidth, knobHeight);
        output.setBounds (p.getX() + 108, p.getY() + 78, knobWidth, knobHeight);
    }
}

//==============================================================================
SatEditor::SatEditor (SatProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    // Read the saved size first: setting the limits below resizes the (still empty) window
    const int width = juce::jlimit (480, 1920, processor.editorWidth > 0 ? processor.editorWidth : SatView::designWidth);

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    view.setBounds (0, 0, SatView::designWidth, SatView::designHeight);

    setResizable (true, true);
    setResizeLimits (480, 300, 1920, 1200);
    if (auto* sizeLimits = getConstrainer())
        sizeLimits->setFixedAspectRatio ((double) SatView::designWidth / SatView::designHeight);

    setSize (width, width * SatView::designHeight / SatView::designWidth);
    sized = true;
}

SatEditor::~SatEditor()
{
    setLookAndFeel (nullptr);
}

void SatEditor::resized()
{
    const float scale = (float) getWidth() / (float) SatView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sized)
        processor.editorWidth = getWidth();
}
