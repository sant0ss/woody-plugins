#include "PluginEditor.h"

using namespace woody;

namespace compwoody
{

BandStrip::BandStrip (CompProcessor& p, int bandIndex)
    : processor (p),
      band (bandIndex),
      threshold (p.parameters, bandId (bandIndex, "thr"), "Limiar", bandColour (bandIndex)),
      ratio (p.parameters, bandId (bandIndex, "ratio"), utf8 ("Razão"), bandColour (bandIndex)),
      attack (p.parameters, bandId (bandIndex, "att"), "Ataque", bandColour (bandIndex)),
      release (p.parameters, bandId (bandIndex, "rel"), utf8 ("Liberação"), bandColour (bandIndex)),
      makeup (p.parameters, bandId (bandIndex, "makeup"), "Ganho", bandColour (bandIndex)),
      soloAttachment (p.parameters, bandId (bandIndex, "solo"), soloButton),
      bypassAttachment (p.parameters, bandId (bandIndex, "bypass"), bypassButton)
{
    for (auto* knob : { &threshold, &ratio, &attack, &release, &makeup })
        addAndMakeVisible (*knob);

    threshold.slider.setTooltip (utf8 ("Nível a partir do qual esta banda começa a ser comprimida."));
    ratio.slider.setTooltip (utf8 ("Quanto o som acima do limiar é reduzido. No máximo, vira limitador."));
    attack.slider.setTooltip (utf8 ("Rapidez com que a compressão começa a agir."));
    release.slider.setTooltip (utf8 ("Rapidez com que o ganho volta ao normal."));
    makeup.slider.setTooltip (utf8 ("Ganho aplicado depois da compressão desta banda."));

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (juce::TextButton::buttonOnColourId, colours::solo);
    soloButton.setTooltip (utf8 ("Ouvir só esta banda"));
    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, colours::mute);
    bypassButton.setTooltip (utf8 ("Deixa esta banda passar sem compressão"));
    addAndMakeVisible (soloButton);
    addAndMakeVisible (bypassButton);

    addAndMakeVisible (meter);
    reductionLabel.setFont (juce::FontOptions (11.5f));
    reductionLabel.setJustificationType (juce::Justification::centred);
    reductionLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (reductionLabel);
}

void BandStrip::update()
{
    const float reduction = processor.getEngine().getBandGainReductionDb (band);
    meter.setReduction (reduction);
    reductionLabel.setText (meter.getReduction() > 0.05f ? "-" + juce::String (meter.getReduction(), 1) : "0.0", juce::dontSendNotification);
}

void BandStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto colour = bandColour (band);
    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + 14.0f, bounds.getY(), 36.0f, 4.0f), 2.0f);
    g.fillEllipse (14.0f, 14.0f, 10.0f, 10.0f);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (bandName (band), juce::Rectangle<float> (32.0f, 8.0f, bounds.getWidth() - 70.0f, 22.0f), juce::Justification::centredLeft);
}

void BandStrip::resized()
{
    constexpr int knobW = 96, knobH = 104;
    const int col0 = 8, col1 = 108;
    const int row0 = 38, row1 = row0 + knobH, row2 = row1 + knobH;

    threshold.setBounds (col0, row0, knobW, knobH);
    ratio.setBounds (col1, row0, knobW, knobH);
    attack.setBounds (col0, row1, knobW, knobH);
    release.setBounds (col1, row1, knobW, knobH);
    makeup.setBounds (col0, row2, knobW, knobH);
    soloButton.setBounds (col1 + 6, row2 + 12, knobW - 12, 30);
    bypassButton.setBounds (col1 + 6, row2 + 54, knobW - 12, 30);

    meter.setBounds (getWidth() - 34, 42, 20, getHeight() - 42 - 40);
    reductionLabel.setBounds (getWidth() - 48, getHeight() - 34, 44, 18);
}
} // namespace compwoody

namespace
{
juce::String peakText (float linear)
{
    const float db = juce::Decibels::gainToDecibels (linear, -80.0f);
    return db < -60.0f ? juce::String ("-inf") : juce::String (db, 1);
}
} // namespace

//==============================================================================
CompView::CompView (CompProcessor& p)
    : processor (p),
      modeSwitch (p.parameters),
      curve (p),
      history (p),
      threshold (p.parameters, "threshold", "Limiar", colours::accent),
      ratio (p.parameters, "ratio", utf8 ("Razão"), colours::accent),
      knee (p.parameters, "knee", "Joelho", colours::accent),
      attack (p.parameters, "attack", "Ataque", colours::accent),
      release (p.parameters, "release", utf8 ("Liberação"), colours::accent),
      makeup (p.parameters, "makeup", "Ganho", colours::accent2),
      mix (p.parameters, "mix", "Mix", colours::accent2),
      scHighPass (p.parameters, "schpf", "Filtro SC", compwoody::blueColour),
      link (p.parameters, "link", utf8 ("Link estéreo"), compwoody::blueColour),
      freqStrip (p),
      mixM (p.parameters, "mix", "Mix", colours::accent2),
      kneeM (p.parameters, "knee", "Joelho", colours::accent),
      linkM (p.parameters, "link", utf8 ("Link estéreo"), compwoody::blueColour)
{
    title.setText ("COMP WOODYS", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    latencyLabel.setFont (juce::FontOptions (12.0f));
    latencyLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (latencyLabel);

    addAndMakeVisible (modeSwitch);
    setupToggle (bypassToggle, bypassAttachment, "Bypass", "bypass", utf8 ("Liga e desliga o efeito, mantendo o atraso do lookahead"));

    threshold.slider.setTooltip (utf8 ("Nível a partir do qual a compressão começa."));
    ratio.slider.setTooltip (utf8 ("Quanto o som acima do limiar é reduzido. No máximo, vira limitador."));
    knee.slider.setTooltip (utf8 ("Suaviza a entrada na compressão. 0 dB é abrupto, valores altos são graduais."));
    attack.slider.setTooltip (utf8 ("Rapidez com que a compressão começa a agir."));
    release.slider.setTooltip (utf8 ("Rapidez com que o ganho volta ao normal."));
    makeup.slider.setTooltip (utf8 ("Ganho aplicado depois da compressão, para recuperar o volume."));
    mix.slider.setTooltip (utf8 ("Mistura o som original com o comprimido (compressão paralela)."));
    scHighPass.slider.setTooltip (utf8 ("Filtro passa-altas no detector: os graves deixam de acionar o compressor. O áudio não é filtrado."));
    link.slider.setTooltip (utf8 ("100% reduz os dois canais igualmente; 0% cada canal comprime sozinho."));
    mixM.slider.setTooltip (mix.slider.getTooltip());
    kneeM.slider.setTooltip (knee.slider.getTooltip());
    linkM.slider.setTooltip (link.slider.getTooltip());

    setupToggle (autoRelease, autoReleaseAttachment, "Auto", "autorelease",
                 utf8 ("A liberação se adapta ao som: rápida em picos curtos, lenta em trechos densos."));
    setupToggle (autoMakeup, autoMakeupAttachment, "Auto", "automakeup", utf8 ("Calcula o ganho de compensação a partir do limiar e da razão."));

    const auto detectorTip = utf8 ("Pico reage a cada transiente; RMS acompanha o volume médio, de forma mais suave.");
    const auto channelTip = utf8 ("Estéreo comprime esquerda e direita; Mid-Side comprime centro e laterais separadamente.");
    const juce::StringArray detectorItems { "Pico", "RMS" };
    const juce::StringArray channelItems { utf8 ("Estéreo"), "Mid-Side" };

    setupBox (detector, "DETECTOR", detectorItems, "detector", detectorTip);
    setupBox (lookahead, "LOOKAHEAD", { "Desligado", "1 ms", "2 ms", "5 ms" }, "lookahead",
              utf8 ("Atrasa o áudio para a compressão agir antes do transiente. Informa a latência ao host."));
    setupBox (channel, "CANAL", channelItems, "channel", channelTip);
    setupBox (detectorM, "DETECTOR", detectorItems, "detector", detectorTip);
    setupBox (channelM, "CANAL", channelItems, "channel", channelTip);

    auto caption = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (label);
    };
    caption (inputCaption, "ENTRADA");
    caption (grCaption, utf8 ("REDUÇÃO"));
    caption (outputCaption, utf8 ("SAÍDA"));

    for (auto* label : { &inputLabel, &grLabel, &outputLabel })
    {
        label->setFont (juce::FontOptions (12.0f));
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, colours::text);
        addAndMakeVisible (label);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &curve, &history, &inputMeter, &outputMeter, &grMeter, &inputLabel, &outputLabel,
                                                           &grLabel, &inputCaption, &outputCaption, &grCaption, &threshold, &ratio, &knee,
                                                           &attack, &release, &makeup, &mix, &scHighPass, &link, &autoRelease, &autoMakeup,
                                                           &detector.caption, &detector.box, &lookahead.caption, &lookahead.box,
                                                           &channel.caption, &channel.box })
    {
        addAndMakeVisible (*c);
        singleComponents.push_back (c);
    }

    for (int b = 0; b < compwoody::numBands; ++b)
    {
        strips[(size_t) b] = std::make_unique<compwoody::BandStrip> (p, b);
        addAndMakeVisible (*strips[(size_t) b]);
        multiComponents.push_back (strips[(size_t) b].get());
    }

    for (auto* c : std::initializer_list<juce::Component*> { &freqStrip, &mixM, &kneeM, &linkM, &detectorM.caption, &detectorM.box,
                                                           &channelM.caption, &channelM.box })
    {
        addAndMakeVisible (*c);
        multiComponents.push_back (c);
    }

    modeSwitch.onChange = [this] (bool isMultiband) { setMode (isMultiband); };
    setMode (processor.getValue ("mode") > 0.5f);
    updateDimming();

    startTimerHz (30);
}

void CompView::setupBox (LabelledBox& labelled, const juce::String& captionText, const juce::StringArray& items, const juce::String& paramId,
                         const juce::String& tip)
{
    for (int i = 0; i < items.size(); ++i)
        labelled.box.addItem (items[i], i + 1);

    labelled.box.setTooltip (tip);
    labelled.attachment = std::make_unique<ComboAttachment> (processor.parameters, paramId, labelled.box);

    labelled.caption.setText (captionText, juce::dontSendNotification);
    labelled.caption.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    labelled.caption.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (labelled.caption);
    addAndMakeVisible (labelled.box);
}

void CompView::setupToggle (juce::ToggleButton& button, std::unique_ptr<ButtonAttachment>& attachment, const juce::String& text,
                            const juce::String& paramId, const juce::String& tip)
{
    button.setButtonText (text);
    button.setTooltip (tip);
    attachment = std::make_unique<ButtonAttachment> (processor.parameters, paramId, button);
    addAndMakeVisible (button);
}

void CompView::setMode (bool isMultiband)
{
    multiband = isMultiband;

    for (auto* c : singleComponents)
        c->setVisible (! multiband);
    for (auto* c : multiComponents)
        c->setVisible (multiband);

    repaint();
}

void CompView::updateDimming()
{
    const bool autoRel = processor.getValue ("autorelease") > 0.5f;
    const bool autoMk = processor.getValue ("automakeup") > 0.5f;

    release.slider.setEnabled (! autoRel);
    release.setAlpha (autoRel ? 0.35f : 1.0f);
    makeup.slider.setEnabled (! autoMk);
    makeup.setAlpha (autoMk ? 0.35f : 1.0f);

    lastAutoRelease = autoRel;
    lastAutoMakeup = autoMk;
}

void CompView::timerCallback()
{
    if ((processor.getValue ("autorelease") > 0.5f) != lastAutoRelease || (processor.getValue ("automakeup") > 0.5f) != lastAutoMakeup)
        updateDimming();

    const auto& engine = processor.getEngine();

    if (multiband)
    {
        for (auto& strip : strips)
            strip->update();

        latencyLabel.setText ({}, juce::dontSendNotification);
        return;
    }

    const float in = processor.takeInputPeak(), out = processor.takeOutputPeak();
    inputMeter.setLevel (in);
    outputMeter.setLevel (out);
    inputHold = juce::jmax (in, inputHold * 0.96f);
    outputHold = juce::jmax (out, outputHold * 0.96f);
    inputLabel.setText (peakText (inputHold), juce::dontSendNotification);
    outputLabel.setText (peakText (outputHold), juce::dontSendNotification);

    grMeter.setReduction (engine.getSingleGainReductionDb());
    grLabel.setText (grMeter.getReduction() > 0.05f ? "-" + juce::String (grMeter.getReduction(), 1) : "0.0", juce::dontSendNotification);

    const int latency = processor.getLatencySamples();
    latencyLabel.setText (latency > 0 ? utf8 ("Latência: ") + juce::String (latency * 1000.0 / processor.getCurrentSampleRate(), 1) + " ms ("
                                            + juce::String (latency) + " amostras)"
                                      : juce::String(),
                          juce::dontSendNotification);
}

void CompView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Logo mark: a wide bar squeezed by two circles
    const float cx = 30.0f, cy = 30.0f;
    g.setColour (colours::accent);
    g.fillRoundedRectangle (cx - 15.0f, cy - 4.0f, 30.0f, 8.0f, 4.0f);
    g.setColour (colours::accent2);
    g.fillEllipse (cx - 14.0f, cy - 14.0f, 12.0f, 12.0f);
    g.fillEllipse (cx + 2.0f, cy + 2.0f, 12.0f, 12.0f);

    for (const auto& group : multiband ? multiGroups : singleGroups)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (group.area.toFloat(), 10.0f);
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (group.title, group.area.getX() + 16, group.area.getY() + 10, group.area.getWidth() - 32, 18, juce::Justification::centredLeft);
    }
}

void CompView::resized()
{
    title.setBounds (56, 12, 240, 34);
    modeSwitch.setBounds (330, 11, 300, 36);
    latencyLabel.setBounds (650, 18, 310, 22);
    bypassToggle.setBounds (972, 16, 108, 28);

    singleGroups.clear();
    multiGroups.clear();

    // Single band: displays on top, controls in three groups below
    curve.setBounds (20, 70, 360, 360);
    history.setBounds (392, 70, 440, 360);

    const juce::Rectangle<int> meters (844, 70, 236, 360);
    singleGroups.push_back ({ meters, utf8 ("NÍVEIS") });
    const int columnWidth = meters.getWidth() / 3;
    juce::Component* bars[3] = { &inputMeter, &grMeter, &outputMeter };
    juce::Label* captions[3] = { &inputCaption, &grCaption, &outputCaption };
    juce::Label* values[3] = { &inputLabel, &grLabel, &outputLabel };
    for (int i = 0; i < 3; ++i)
    {
        const int x = meters.getX() + i * columnWidth;
        captions[i]->setBounds (x, meters.getY() + 38, columnWidth, 16);
        bars[i]->setBounds (x + columnWidth / 2 - 15, meters.getY() + 62, 30, meters.getHeight() - 62 - 48);
        values[i]->setBounds (x, meters.getBottom() - 36, columnWidth, 20);
    }

    const juce::Rectangle<int> compression (20, 444, 520, 246), output (552, 444, 230, 246), detection (794, 444, 286, 246);
    singleGroups.push_back ({ compression, utf8 ("COMPRESSÃO") });
    singleGroups.push_back ({ output, utf8 ("SAÍDA") });
    singleGroups.push_back ({ detection, utf8 ("DETECÇÃO") });

    constexpr int knobW = 96, knobH = 120;
    const int knobY = compression.getY() + 40;
    woody::ParamKnob* compressionKnobs[5] = { &threshold, &ratio, &knee, &attack, &release };
    for (int i = 0; i < 5; ++i)
        compressionKnobs[i]->setBounds (compression.getX() + 14 + i * 100, knobY, knobW, knobH);
    autoRelease.setBounds (compression.getX() + 14 + 400 + 8, knobY + knobH + 8, knobW - 8, 24);

    makeup.setBounds (output.getX() + 14, knobY, knobW, knobH);
    mix.setBounds (output.getX() + 120, knobY, knobW, knobH);
    autoMakeup.setBounds (output.getX() + 14 + 8, knobY + knobH + 8, knobW - 8, 24);

    scHighPass.setBounds (detection.getX() + 14, knobY, knobW, knobH);
    link.setBounds (detection.getX() + 120, knobY, knobW, knobH);

    auto placeBox = [] (LabelledBox& labelled, int x, int y, int w)
    {
        labelled.caption.setBounds (x, y, w, 14);
        labelled.box.setBounds (x, y + 16, w, 28);
    };
    const int boxY = knobY + knobH + 6;
    placeBox (detector, detection.getX() + 14, boxY, 80);
    placeBox (lookahead, detection.getX() + 102, boxY, 80);
    placeBox (channel, detection.getX() + 190, boxY, 82);

    // Multiband: frequency strip on top, one strip per band below
    freqStrip.setBounds (20, 70, 800, 250);

    const juce::Rectangle<int> general (832, 70, 248, 250);
    multiGroups.push_back ({ general, "GERAL" });
    mixM.setBounds (general.getX() + 10, general.getY() + 38, 76, 100);
    kneeM.setBounds (general.getX() + 86, general.getY() + 38, 76, 100);
    linkM.setBounds (general.getX() + 162, general.getY() + 38, 76, 100);
    placeBox (detectorM, general.getX() + 12, general.getY() + 164, 108);
    placeBox (channelM, general.getX() + 128, general.getY() + 164, 108);

    for (int b = 0; b < compwoody::numBands; ++b)
        strips[(size_t) b]->setBounds (20 + b * (253 + 16), 332, 253, 358);
}

//==============================================================================
CompEditor::CompEditor (CompProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    // Read the saved size first: setting the limits below resizes the (still empty) window
    const int width = juce::jlimit (770, 1980, processor.editorWidth > 0 ? processor.editorWidth : CompView::designWidth);

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    view.setBounds (0, 0, CompView::designWidth, CompView::designHeight);

    setResizable (true, true);
    setResizeLimits (770, 490, 1980, 1260);
    if (auto* sizeLimits = getConstrainer())
        sizeLimits->setFixedAspectRatio ((double) CompView::designWidth / CompView::designHeight);

    setSize (width, width * CompView::designHeight / CompView::designWidth);
    sized = true;
}

CompEditor::~CompEditor()
{
    setLookAndFeel (nullptr);
}

void CompEditor::resized()
{
    const float scale = (float) getWidth() / (float) CompView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sized)
        processor.editorWidth = getWidth();
}
