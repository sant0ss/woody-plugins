#include "PluginEditor.h"

using namespace synthwoody;
using namespace woody;

namespace
{
const juce::Colour oscAColour = colours::accent;
const juce::Colour oscBColour = colours::accent2;
const juce::Colour mixColour { 0xffc9a7ff };
const juce::Colour filterColour { 0xffffd23d };
const juce::Colour ampColour { 0xff5fe39a };
const juce::Colour fltEnvColour { 0xff4da3ff };
const juce::Colour modColour { 0xffff5ea8 };
const juce::Colour fxColour { 0xff8a7dff };
} // namespace

SynthView::SynthView (SynthWoodysProcessor& p, juce::LookAndFeel& lnf)
    : processor (p), lookAndFeel (lnf),
      analogView (p.parameters), wavetableView (p.parameters), filterView (p.parameters),
      ampEnvView (p.parameters, ampA, ampD, ampS, ampR, ampColour),
      fltEnvView (p.parameters, fltA, fltD, fltS, fltR, fltEnvColour),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // Panels
    addPanel ("OSC A  -  ANALÓGICO", { 12, 60, 272, 250 }, oscAColour);
    addPanel ("OSC B  -  WAVETABLE", { 296, 60, 296, 250 }, oscBColour);
    addPanel ("UNISON  E  MIXAGEM", { 604, 60, 236, 250 }, mixColour);
    addPanel ("FILTRO", { 852, 60, 316, 250 }, filterColour);
    addPanel ("ENVELOPE DE VOLUME", { 12, 322, 232, 300 }, ampColour);
    addPanel ("ENVELOPE DO FILTRO", { 256, 322, 232, 300 }, fltEnvColour);
    addPanel ("LFOS  E  MODULAÇÃO", { 500, 322, 360, 300 }, modColour);
    addPanel ("EFEITOS", { 872, 322, 296, 300 }, fxColour);

    // Header
    title.setText ("SYNTH WOODYS", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    subtitle.setText ("woody plugins", juce::dontSendNotification);
    subtitle.setFont (juce::FontOptions (12.0f));
    subtitle.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (subtitle);

    presetBox.setTooltip (utf8 ("Sons de fábrica e os seus"));
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1000 && id - 1000 < (int) userPresets.size())
            processor.applyUserPreset (userPresets[(size_t) (id - 1000)]);
        else if (id >= 1 && id <= (int) factoryPresets().size())
            processor.applyFactoryPreset (factoryPresets()[(size_t) (id - 1)]);
        deleteButton.setEnabled (id >= 1000);
    };
    addAndMakeVisible (presetBox);

    saveButton.setTooltip ("Guarda o som atual com um nome");
    saveButton.onClick = [this] { savePreset(); };
    deleteButton.onClick = [this] { deletePreset(); };
    panicButton.setTooltip (utf8 ("Corta todas as notas que estiverem soando"));
    panicButton.onClick = [this] { processor.panic(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (panicButton);

    // Oscillator A
    addCombo (aShape);
    addAndMakeVisible (analogView);
    addKnob (aPw, "Pulso", oscAColour);
    addKnob (aSemi, "Semitom", oscAColour);
    addKnob (aFine, "Fino", oscAColour);
    addKnob (aLevel, utf8 ("Nível"), oscAColour);

    // Oscillator B
    addCombo (bTable);
    addAndMakeVisible (wavetableView);
    addKnob (bPos, utf8 ("Posição"), oscBColour);
    addKnob (bSemi, "Semitom", oscBColour);
    addKnob (bFine, "Fino", oscBColour);
    addKnob (bLevel, utf8 ("Nível"), oscBColour);

    // Unison and mix
    addKnob (unison, "Vozes", mixColour);
    addKnob (uniDetune, "Desafinar", mixColour);
    addKnob (uniSpread, "Abertura", mixColour);
    addKnob (subLevel, "Sub", mixColour);
    addKnob (noiseLevel, utf8 ("Ruído"), mixColour);

    // Filter
    addCombo (fType);
    addAndMakeVisible (filterView);
    addKnob (cutoff, "Corte", filterColour);
    addKnob (res, "Resson.", filterColour);
    addKnob (fDrive, "Drive", filterColour);
    addKnob (envAmt, "Envelope", filterColour);
    addKnob (keytrack, "Teclado", filterColour);

    // Envelopes
    addAndMakeVisible (ampEnvView);
    addKnob (ampA, "Ataque", ampColour);
    addKnob (ampD, "Queda", ampColour);
    addKnob (ampS, "Sustain", ampColour);
    addKnob (ampR, "Soltar", ampColour);

    addAndMakeVisible (fltEnvView);
    addKnob (fltA, "Ataque", fltEnvColour);
    addKnob (fltD, "Queda", fltEnvColour);
    addKnob (fltS, "Sustain", fltEnvColour);
    addKnob (fltR, "Soltar", fltEnvColour);

    // LFOs and modulation matrix
    for (auto* l : { &lfo1Label, &lfo2Label })
    {
        l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (*l);
    }
    lfo1Label.setText ("LFO 1", juce::dontSendNotification);
    lfo2Label.setText ("LFO 2", juce::dontSendNotification);

    addCombo (lfo1Shape);
    addKnob (lfo1Rate, "Taxa", modColour);
    addCombo (lfo2Shape);
    addKnob (lfo2Rate, "Taxa", modColour);

    for (int slot = 0; slot < numModSlots; ++slot)
    {
        const int first = (int) m1Src + 3 * slot;
        addCombo ((ParamIndex) first);
        addCombo ((ParamIndex) (first + 1));
        addAmountSlider ((ParamIndex) (first + 2));
    }

    // Effects
    addKnob (fxDrive, "Drive", fxColour);
    addKnob (chMix, "Chorus", fxColour);
    addKnob (chRate, "Taxa", fxColour);
    addKnob (chDepth, "Profund.", fxColour);
    addKnob (dlMix, "Delay", fxColour);
    addKnob (dlTime, "Tempo", fxColour);
    addKnob (dlFb, "Feedback", fxColour);
    addKnob (rvMix, "Reverb", fxColour);
    addKnob (rvSize, "Tamanho", fxColour);
    addKnob (master, "Master", colours::text);

    // On-screen keyboard to play without a controller
    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (32.0f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdfe3ee));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, colours::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, colours::line);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, colours::textDim);
    addAndMakeVisible (keyboard);

    refreshPresets();
    startTimerHz (24);
}

void SynthView::addPanel (const char* utf8Title, juce::Rectangle<int> bounds, juce::Colour colour)
{
    panels.push_back ({ juce::String::fromUTF8 (utf8Title), bounds, colour });
}

Knob& SynthView::addKnob (ParamIndex param, const juce::String& caption, juce::Colour colour)
{
    knobs[(size_t) param] = std::make_unique<Knob> (processor.parameters, param, caption, colour);
    addAndMakeVisible (*knobs[(size_t) param]);
    return *knobs[(size_t) param];
}

juce::ComboBox& SynthView::addCombo (ParamIndex param)
{
    auto combo = std::make_unique<juce::ComboBox>();
    const auto names = choicesOf (param);
    for (int i = 0; i < names.size(); ++i)
        combo->addItem (names[i], i + 1);

    comboAttachments[(size_t) param] = std::make_unique<ComboAttachment> (processor.parameters, paramDefs()[(size_t) param].id, *combo);
    addAndMakeVisible (*combo);

    combos[(size_t) param] = std::move (combo);
    return *combos[(size_t) param];
}

juce::Slider& SynthView::addAmountSlider (ParamIndex param)
{
    auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    slider->setColour (juce::Slider::trackColourId, modColour);
    slider->setPopupDisplayEnabled (true, true, nullptr);
    slider->setDoubleClickReturnValue (true, 0.0);
    sliderAttachments[(size_t) param] = std::make_unique<SliderAttachment> (processor.parameters, paramDefs()[(size_t) param].id, *slider);
    addAndMakeVisible (*slider);

    amountSliders[(size_t) param] = std::move (slider);
    return *amountSliders[(size_t) param];
}

//==============================================================================
void SynthView::refreshPresets()
{
    userPresets = SynthWoodysProcessor::loadUserPresets();
    presetBox.clear (juce::dontSendNotification);

    int selectedId = 0;
    presetBox.addSectionHeading (utf8 ("Sons de fábrica"));
    for (size_t i = 0; i < factoryPresets().size(); ++i)
    {
        presetBox.addItem (factoryPresets()[i].name, (int) i + 1);
        if (processor.currentPresetName == factoryPresets()[i].name)
            selectedId = (int) i + 1;
    }

    if (! userPresets.empty())
    {
        presetBox.addSectionHeading ("Meus sons");
        for (size_t i = 0; i < userPresets.size(); ++i)
        {
            const auto name = userPresets[i].getProperty ("name").toString();
            presetBox.addItem (name, 1000 + (int) i);
            if (processor.currentPresetName == name)
                selectedId = 1000 + (int) i;
        }
    }

    presetBox.setTextWhenNothingSelected ("Som personalizado");
    presetBox.setSelectedId (selectedId, juce::dontSendNotification);
    deleteButton.setEnabled (selectedId >= 1000);
    shownPreset = processor.currentPresetName;
}

void SynthView::savePreset()
{
    auto* window = new juce::AlertWindow ("Salvar som", utf8 ("Dê um nome para este som:"), juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&lookAndFeel);
    window->addTextEditor ("name", processor.currentPresetName.isNotEmpty() ? processor.currentPresetName : juce::String ("Meu som"));
    window->addButton ("Salvar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, window] (int result)
        {
            const auto name = window->getTextEditorContents ("name").trim();
            if (result != 1 || name.isEmpty())
                return;

            auto presets = SynthWoodysProcessor::loadUserPresets();
            auto preset = processor.capturePreset (name);
            auto existing = std::find_if (presets.begin(), presets.end(),
                                          [&] (const juce::ValueTree& t) { return t.getProperty ("name").toString() == name; });
            if (existing != presets.end())
                *existing = preset;
            else
                presets.push_back (preset);

            SynthWoodysProcessor::saveUserPresets (presets);
            processor.currentPresetName = name;
            refreshPresets();
        }), true);
}

void SynthView::deletePreset()
{
    const int index = presetBox.getSelectedId() - 1000;
    if (! juce::isPositiveAndBelow (index, (int) userPresets.size()))
        return;

    userPresets.erase (userPresets.begin() + index);
    SynthWoodysProcessor::saveUserPresets (userPresets);
    processor.currentPresetName = {};
    refreshPresets();
}

//==============================================================================
void SynthView::timerCallback()
{
    analogView.repaint();
    wavetableView.repaint();
    filterView.repaint();
    ampEnvView.repaint();
    fltEnvView.repaint();

    // Keep the preset box in step when the host or a preset load changes the name
    if (processor.currentPresetName != shownPreset)
        refreshPresets();

    repaint (juce::Rectangle<int> (1000, 14, 168, 28));
}

void SynthView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Logo mark: three offset circles
    const float cx = 34.0f, cy = 34.0f;
    g.setColour (colours::accent);
    g.fillEllipse (cx - 13.0f, cy - 13.0f, 18.0f, 18.0f);
    g.setColour (colours::accent2.withAlpha (0.85f));
    g.fillEllipse (cx - 3.0f, cy - 7.0f, 14.0f, 14.0f);
    g.setColour (colours::text.withAlpha (0.8f));
    g.fillEllipse (cx - 9.0f, cy + 3.0f, 8.0f, 8.0f);

    for (const auto& panel : panels)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (panel.bounds.toFloat(), 14.0f);

        g.setColour (panel.colour);
        g.fillRoundedRectangle ((float) panel.bounds.getX() + 14.0f, (float) panel.bounds.getY() + 16.0f, 4.0f, 16.0f, 2.0f);

        g.setColour (colours::text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (panel.title, panel.bounds.getX() + 26, panel.bounds.getY() + 12, panel.bounds.getWidth() - 40, 24,
                    juce::Justification::centredLeft);
    }

    // Column headings of the modulation matrix
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ORIGEM", 514, 434, 118, 14, juce::Justification::centredLeft);
    g.drawText ("DESTINO", 640, 434, 124, 14, juce::Justification::centredLeft);
    g.drawText ("QUANTIDADE", 772, 434, 80, 14, juce::Justification::centredLeft);

    // Output level
    const float peak = processor.getOutputPeak();
    const float db = juce::Decibels::gainToDecibels (peak, -48.0f);
    const float level = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    const juce::Rectangle<float> meter (1000.0f, 26.0f, 168.0f, 8.0f);

    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (meter, 4.0f);
    if (level > 0.0f)
    {
        juce::ColourGradient gradient (colours::accent2, meter.getX(), 0.0f, colours::mute, meter.getRight(), 0.0f, false);
        gradient.addColour (0.75, colours::solo);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * level), 4.0f);
    }
}

void SynthView::resized()
{
    auto place = [] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); };
    auto knob = [&] (ParamIndex p, int x, int y, int w, int h = 80) { place (*knobs[(size_t) p], x, y, w, h); };
    auto combo = [&] (ParamIndex p, int x, int y, int w, int h = 28) { place (*combos[(size_t) p], x, y, w, h); };

    // Header
    place (title, 50, 8, 200, 26);
    place (subtitle, 50, 32, 200, 16);
    place (presetBox, 270, 12, 280, 32);
    place (saveButton, 560, 12, 84, 32);
    place (deleteButton, 652, 12, 84, 32);
    place (panicButton, 744, 12, 110, 32);

    // Oscillator A
    combo (aShape, 26, 96, 244);
    place (analogView, 26, 130, 244, 76);
    for (auto [p, i] : { std::pair { aPw, 0 }, { aSemi, 1 }, { aFine, 2 }, { aLevel, 3 } })
        knob (p, 26 + i * 61, 214, 61);

    // Oscillator B
    combo (bTable, 310, 96, 268);
    place (wavetableView, 310, 130, 268, 76);
    for (auto [p, i] : { std::pair { bPos, 0 }, { bSemi, 1 }, { bFine, 2 }, { bLevel, 3 } })
        knob (p, 310 + i * 67, 214, 67);

    // Unison and mix
    for (auto [p, i] : { std::pair { unison, 0 }, { uniDetune, 1 }, { uniSpread, 2 } })
        knob (p, 618 + i * 70, 100, 70, 90);
    for (auto [p, i] : { std::pair { subLevel, 0 }, { noiseLevel, 1 } })
        knob (p, 618 + i * 70, 200, 70, 90);

    // Filter
    combo (fType, 866, 96, 288);
    place (filterView, 866, 130, 288, 76);
    for (auto [p, i] : { std::pair { cutoff, 0 }, { res, 1 }, { fDrive, 2 }, { envAmt, 3 }, { keytrack, 4 } })
        knob (p, 866 + i * 58, 214, 58);

    // Envelopes
    place (ampEnvView, 26, 360, 204, 130);
    for (auto [p, i] : { std::pair { ampA, 0 }, { ampD, 1 }, { ampS, 2 }, { ampR, 3 } })
        knob (p, 26 + i * 51, 504, 51, 84);

    place (fltEnvView, 270, 360, 204, 130);
    for (auto [p, i] : { std::pair { fltA, 0 }, { fltD, 1 }, { fltS, 2 }, { fltR, 3 } })
        knob (p, 270 + i * 51, 504, 51, 84);

    // LFOs
    place (lfo1Label, 514, 354, 90, 16);
    combo (lfo1Shape, 514, 374, 90);
    knob (lfo1Rate, 610, 352, 62, 76);

    place (lfo2Label, 692, 354, 90, 16);
    combo (lfo2Shape, 692, 374, 90);
    knob (lfo2Rate, 788, 352, 62, 76);

    // Modulation matrix
    for (int slot = 0; slot < numModSlots; ++slot)
    {
        const int first = (int) m1Src + 3 * slot;
        const int y = 452 + slot * 40;
        combo ((ParamIndex) first, 514, y, 118);
        combo ((ParamIndex) (first + 1), 640, y, 124);
        place (*amountSliders[(size_t) (first + 2)], 772, y, 74, 28);
    }

    // Effects
    const int fxX = 886, fxW = 67;
    for (auto [p, i] : { std::pair { fxDrive, 0 }, { chMix, 1 }, { chRate, 2 }, { chDepth, 3 } })
        knob (p, fxX + i * fxW, 358, fxW, 78);
    for (auto [p, i] : { std::pair { dlMix, 0 }, { dlTime, 1 }, { dlFb, 2 } })
        knob (p, fxX + i * fxW, 438, fxW, 78);
    for (auto [p, i] : { std::pair { rvMix, 0 }, { rvSize, 1 }, { master, 3 } })
        knob (p, fxX + i * fxW, 518, fxW, 78);

    place (keyboard, 12, 634, 1156, 98);
}

//==============================================================================
SynthWoodysEditor::SynthWoodysEditor (SynthWoodysProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p, lookAndFeel)
{
    // Read the saved size first: setting the limits below resizes the (still empty) window
    const int width = juce::jlimit (840, 1920, processor.editorWidth > 0 ? processor.editorWidth : SynthView::designWidth);

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    view.setBounds (0, 0, SynthView::designWidth, SynthView::designHeight);

    // Free resizing with the design's proportions; the content scales with the window
    setResizable (true, true);
    setResizeLimits (840, 527, 1920, 1204);
    if (auto* sizeLimits = getConstrainer())
        sizeLimits->setFixedAspectRatio ((double) SynthView::designWidth / SynthView::designHeight);

    setSize (width, width * SynthView::designHeight / SynthView::designWidth);
    sized = true;
}

SynthWoodysEditor::~SynthWoodysEditor()
{
    setLookAndFeel (nullptr);
}

void SynthWoodysEditor::resized()
{
    const float scale = (float) getWidth() / (float) SynthView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sized)
        processor.editorWidth = getWidth();
}
