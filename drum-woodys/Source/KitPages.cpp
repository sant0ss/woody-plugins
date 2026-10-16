#include "Pages.h"
#include "PageHelpers.h"

namespace woody
{
using namespace helpers;

//==============================================================================
void SimpleListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (4.0f, 2.0f);
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (bounds.withWidth (3.0f).reduced (0.0f, 6.0f), 1.5f);
    }

    auto textArea = bounds.reduced (12.0f, 0.0f).toNearestInt();
    if (detail != nullptr)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (detail (row), textArea.removeFromRight (80), juce::Justification::centredRight);
    }

    g.setColour (selected ? colours::text : colours::text.withAlpha (0.8f));
    g.setFont (juce::FontOptions (14.0f, selected ? juce::Font::bold : juce::Font::plain));
    if (text != nullptr)
        g.drawText (text (row), textArea, juce::Justification::centredLeft);
}

void SimpleListModel::selectedRowsChanged (int lastRowSelected)
{
    if (onSelect != nullptr && lastRowSelected >= 0)
        onSelect (lastRowSelected);
}

void SimpleListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (onDoubleClick != nullptr)
        onDoubleClick (row);
}

SectionLabel::SectionLabel (const juce::String& text)
{
    setText (text, juce::dontSendNotification);
    setFont (juce::FontOptions (11.0f, juce::Font::bold));
    setColour (juce::Label::textColourId, colours::textDim);
}

//==============================================================================
KitMap::KitMap (DrumWoodysProcessor& p) : processor (p) {}

const std::array<KitMap::Shape, numSlots>& KitMap::shapes()
{
    static const std::array<Shape, numSlots> s {{
        { 405, 330, 92 }, // kick
        { 230, 320, 56 }, // snare
        { 100, 235, 52 }, // hi-hat
        { 310, 160, 46 }, // tom 1
        { 455, 150, 50 }, // tom 2
        { 590, 320, 64 }, // floor
        { 175,  90, 62 }, // crash 1
        { 615,  80, 58 }, // crash 2
        { 715, 215, 72 }, // ride
        { 735, 385, 46 }, // fx
        {  40, 420, 22 }, // perc 1-4
        {  92, 420, 22 },
        { 144, 420, 22 },
        { 196, 420, 22 },
    }};
    return s;
}

juce::AffineTransform KitMap::canvasTransform() const
{
    constexpr float canvasW = 800.0f, canvasH = 460.0f;
    const float scale = juce::jmin ((float) getWidth() / canvasW, (float) getHeight() / canvasH);
    const float dx = ((float) getWidth() - canvasW * scale) / 2.0f;
    const float dy = ((float) getHeight() - canvasH * scale) / 2.0f;
    return juce::AffineTransform::scale (scale).translated (dx, dy);
}

juce::Rectangle<float> KitMap::shapeBounds (int slot) const
{
    const auto& s = shapes()[(size_t) slot];
    return juce::Rectangle<float> (s.x - s.radius, s.y - s.radius, s.radius * 2.0f, s.radius * 2.0f).transformedBy (canvasTransform());
}

int KitMap::slotAt (juce::Point<float> p) const
{
    // Smallest shapes first, so percussion and toms win over the big kick and cymbals
    int best = -1;
    float bestRadius = 1.0e9f;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto b = shapeBounds (s);
        if (b.getCentre().getDistanceFrom (p) <= b.getWidth() / 2.0f && b.getWidth() < bestRadius)
        {
            best = s;
            bestRadius = b.getWidth();
        }
    }
    return best;
}

void KitMap::setSelected (int slot)
{
    selected = slot;
    repaint();
}

void KitMap::flash (int slot)
{
    flashLevel[(size_t) slot] = 1.0f;
}

void KitMap::tick()
{
    bool changed = false;
    for (auto& f : flashLevel)
    {
        if (f > 0.0f)
        {
            f = juce::jmax (0.0f, f - 0.07f);
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void KitMap::paint (juce::Graphics& g)
{
    // Quiet dot grid as a backdrop
    g.setColour (colours::line.withAlpha (0.5f));
    for (int y = 12; y < getHeight(); y += 24)
        for (int x = 12; x < getWidth(); x += 24)
            g.fillRect ((float) x, (float) y, 1.5f, 1.5f);

    const auto library = processor.getLibrary();

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& def = slotDefs()[(size_t) s];
        const auto b = shapeBounds (s);
        const auto c = slotColour (s);
        const float flash = flashLevel[(size_t) s];
        const bool empty = library->find (processor.getSlotInstrument (s)) == nullptr;
        const float alpha = empty ? 0.35f : 1.0f;
        const bool cymbal = def.type == crashType || def.type == rideType || def.type == fxType || def.type == hihatType;

        // Expanding ring when hit
        if (flash > 0.0f)
        {
            const float grow = b.getWidth() * 0.18f * (1.0f - flash);
            g.setColour (c.withAlpha (flash * 0.8f));
            g.drawEllipse (b.expanded (grow + 4.0f), 2.0f);
        }

        if (def.type == percType)
        {
            juce::Path diamond;
            diamond.addRectangle (b.reduced (b.getWidth() * 0.15f));
            diamond.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi / 4.0f, b.getCentreX(), b.getCentreY()));
            g.setColour (c.withAlpha ((0.25f + flash * 0.6f) * alpha));
            g.fillPath (diamond);
            g.setColour (c.withAlpha (alpha));
            g.strokePath (diamond, juce::PathStrokeType (1.6f));
        }
        else if (cymbal)
        {
            g.setColour (c.withAlpha ((0.06f + flash * 0.35f) * alpha));
            g.fillEllipse (b);
            g.setColour (c.withAlpha (alpha));
            g.drawEllipse (b.reduced (1.5f), 3.0f);
            if (def.type == hihatType)
                g.drawEllipse (b.reduced (b.getWidth() * 0.16f), 1.5f);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (b.getCentre()));
        }
        else
        {
            g.setColour (c.withAlpha ((0.16f + flash * 0.55f) * alpha));
            g.fillEllipse (b);
            g.setColour (c.withAlpha (alpha));
            g.drawEllipse (b.reduced (1.0f), 2.0f);
            g.setColour (c.withAlpha (0.35f * alpha));
            g.drawEllipse (b.reduced (b.getWidth() * 0.24f), 1.2f);
        }

        if (s == selected)
        {
            g.setColour (colours::text);
            g.drawEllipse (b.expanded (5.0f), 1.5f);
        }

        g.setColour (s == selected ? colours::text : colours::textDim);
        g.setFont (juce::FontOptions (def.type == percType ? 10.5f : 12.0f, s == selected ? juce::Font::bold : juce::Font::plain));
        const auto labelArea = juce::Rectangle<float> (b.getCentreX() - 60.0f, b.getBottom() + 3.0f, 120.0f, 16.0f);
        g.drawText (def.name, labelArea, juce::Justification::centredTop);
    }
}

void KitMap::mouseDown (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.position);
    if (slot < 0)
        return;

    // Closer to the centre plays louder
    const auto b = shapeBounds (slot);
    const float distance = b.getCentre().getDistanceFrom (e.position) / (b.getWidth() / 2.0f);
    const float velocity = juce::jlimit (0.3f, 1.0f, 1.0f - distance * 0.6f);

    heldNote = slotDefs()[(size_t) slot].articulations[0].note;
    processor.keyboardState.noteOn (1, heldNote, velocity);

    setSelected (slot);
    if (onSelect != nullptr)
        onSelect (slot);
}

void KitMap::mouseUp (const juce::MouseEvent&)
{
    if (heldNote >= 0)
        processor.keyboardState.noteOff (1, heldNote, 0.0f);
    heldNote = -1;
}

//==============================================================================
ArticulationPad::ArticulationPad (DrumWoodysProcessor& p, int slot, int articulation)
    : processor (p), colour (slotColour (slot))
{
    const auto& def = slotDefs()[(size_t) slot].articulations[(size_t) articulation];
    note = def.note;
    label = juce::String (def.name);
    setTooltip (label + " - nota MIDI " + juce::String (note) + " (" + noteName (note) + ")");
}

void ArticulationPad::decay()
{
    if (flashLevel > 0.0f)
    {
        flashLevel = juce::jmax (0.0f, flashLevel - 0.1f);
        repaint();
    }
}

void ArticulationPad::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (colours::panelLight.interpolatedWith (colour, 0.12f + flashLevel * 0.6f));
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (colour);
    g.fillEllipse (bounds.getX() + 10.0f, bounds.getCentreY() - 3.0f, 6.0f, 6.0f);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (label, bounds.withTrimmedLeft (24.0f), juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (noteName (note) + "  " + juce::String (note), bounds.reduced (10.0f, 0.0f), juce::Justification::centredRight);
}

void ArticulationPad::mouseDown (const juce::MouseEvent& e)
{
    const float height = (float) juce::jmax (1, getHeight());
    const float velocity = juce::jlimit (0.25f, 1.0f, 1.0f - (float) e.y / height * 0.75f);
    processor.keyboardState.noteOn (1, note, velocity);
    flash();
}

void ArticulationPad::mouseUp (const juce::MouseEvent&)
{
    processor.keyboardState.noteOff (1, note, 0.0f);
}

//==============================================================================
SlotPanel::SlotPanel (DrumWoodysProcessor& p, int s)
    : processor (p), slot (s), colour (slotColour (s)),
      level    (p.parameters, DrumWoodysProcessor::paramId (s, "level"), "Volume", slotColour (s)),
      pan      (p.parameters, DrumWoodysProcessor::paramId (s, "pan"), "Pan", slotColour (s)),
      tune     (p.parameters, DrumWoodysProcessor::paramId (s, "tune"), utf8 ("Afinação"), slotColour (s)),
      overhead (p.parameters, DrumWoodysProcessor::paramId (s, "oh"), "Overhead", slotColour (s)),
      room     (p.parameters, DrumWoodysProcessor::paramId (s, "room"), "Room", slotColour (s)),
      eqLow    (p.parameters, DrumWoodysProcessor::paramId (s, "eqlow"), "Graves", slotColour (s)),
      eqMid    (p.parameters, DrumWoodysProcessor::paramId (s, "eqmid"), utf8 ("Médios"), slotColour (s)),
      eqFreq   (p.parameters, DrumWoodysProcessor::paramId (s, "eqfreq"), utf8 ("Freq. médios"), slotColour (s)),
      eqHigh   (p.parameters, DrumWoodysProcessor::paramId (s, "eqhigh"), "Agudos", slotColour (s)),
      comp     (p.parameters, DrumWoodysProcessor::paramId (s, "comp"), "Compressor", slotColour (s)),
      attack   (p.parameters, DrumWoodysProcessor::paramId (s, "attack"), "Ataque", slotColour (s)),
      sustain  (p.parameters, DrumWoodysProcessor::paramId (s, "sustain"), "Sustain", slotColour (s)),
      drive    (p.parameters, DrumWoodysProcessor::paramId (s, "drive"), "Drive", slotColour (s))
{
    const auto& def = slotDefs()[(size_t) slot];

    title.setText (def.name, juce::dontSendNotification);
    styleTitle (title, 20.0f);
    addAndMakeVisible (title);

    mute.setClickingTogglesState (true);
    mute.setColour (juce::TextButton::buttonOnColourId, colours::mute);
    mute.setTooltip ("Mudo");
    solo.setClickingTogglesState (true);
    solo.setColour (juce::TextButton::buttonOnColourId, colours::solo);
    solo.setTooltip ("Solo");
    muteAttachment = std::make_unique<ButtonAttachment> (p.parameters, DrumWoodysProcessor::paramId (s, "mute"), mute);
    soloAttachment = std::make_unique<ButtonAttachment> (p.parameters, DrumWoodysProcessor::paramId (s, "solo"), solo);
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);

    piece.setTooltip (utf8 ("Escolha a peça para esta posição, de qualquer kit"));
    piece.onChange = [this]
    {
        const int index = piece.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) itemIds.size()) && itemIds[(size_t) index] != processor.getSlotInstrument (slot))
            processor.setSlotInstrument (slot, itemIds[(size_t) index]);
    };
    addAndMakeVisible (piece);

    for (int a = 0; a < def.numArticulations; ++a)
    {
        pads.push_back (std::make_unique<ArticulationPad> (processor, slot, a));
        addAndMakeVisible (*pads.back());
    }

    for (auto* l : { &mixLabel, &eqLabel, &dynamicsLabel })
        addAndMakeVisible (*l);

    overhead.setTooltip (utf8 ("Quanto desta peça os microfones de cima captam"));
    room.setTooltip (utf8 ("Quanto desta peça vai para o som da sala"));
    attack.setTooltip (utf8 ("Realça ou suaviza o início da batida"));
    sustain.setTooltip (utf8 ("Deixa a peça soar mais longa ou mais curta"));
    drive.setTooltip (utf8 ("Saturação: deixa o som mais quente e agressivo"));

    for (auto* k : { &level, &pan, &tune, &overhead, &room, &eqLow, &eqMid, &eqFreq, &eqHigh, &comp, &attack, &sustain, &drive })
        addAndMakeVisible (*k);

    separateOut.setButtonText (utf8 ("Saída separada no Ableton"));
    separateOut.setTooltip (utf8 ("Manda esta peça para uma saída própria do plugin, para mixar numa faixa separada do Ableton"));
    outAttachment = std::make_unique<ButtonAttachment> (p.parameters, DrumWoodysProcessor::paramId (s, "out"), separateOut);
    addAndMakeVisible (separateOut);

    refresh();
}

void SlotPanel::refresh()
{
    piece.clear (juce::dontSendNotification);
    itemIds.clear();

    const auto library = processor.getLibrary();
    const auto current = processor.getSlotInstrument (slot);
    juce::String lastKit;
    int selectedId = 0;

    for (const auto* inst : library->instrumentsFor (slot))
    {
        if (inst->kitName != lastKit)
        {
            piece.addSectionHeading (inst->kitName);
            lastKit = inst->kitName;
        }

        itemIds.push_back (inst->id);
        const int id = (int) itemIds.size();
        piece.addItem (inst->name.fromFirstOccurrenceOf (" - ", false, false), id);

        if (inst->id == current)
            selectedId = id;
    }

    piece.setTextWhenNothingSelected (itemIds.empty() ? utf8 ("(sem peças na biblioteca)") : utf8 ("(nenhuma)"));
    piece.setSelectedId (selectedId, juce::dontSendNotification);

    if (const auto* inst = library->find (current))
        piece.setTooltip (inst->name);
}

void SlotPanel::decay()
{
    for (auto& pad : pads)
        pad->decay();
}

void SlotPanel::paint (juce::Graphics& g)
{
    fillPanel (g, getLocalBounds());

    // Colour accent next to the title
    g.setColour (colour);
    g.fillRoundedRectangle (16.0f, 22.0f, 4.0f, 22.0f, 2.0f);
}

void SlotPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 16);

    auto header = area.removeFromTop (34);
    solo.setBounds (header.removeFromRight (34).withSizeKeepingCentre (34, 26));
    header.removeFromRight (6);
    mute.setBounds (header.removeFromRight (34).withSizeKeepingCentre (34, 26));
    title.setBounds (header.withTrimmedLeft (12));

    area.removeFromTop (8);
    piece.setBounds (area.removeFromTop (32));
    area.removeFromTop (10);

    const int padRows = ((int) pads.size() + 1) / 2;
    auto padArea = area.removeFromTop (padRows * 36);
    const int padWidth = padArea.getWidth() / 2;
    for (size_t i = 0; i < pads.size(); ++i)
        pads[i]->setBounds (padArea.getX() + (int) (i % 2) * padWidth, padArea.getY() + (int) (i / 2) * 36, padWidth, 36);

    auto knobRow = [&] (SectionLabel& label, std::initializer_list<LabeledKnob*> knobs)
    {
        area.removeFromTop (10);
        label.setBounds (area.removeFromTop (16));
        auto row = area.removeFromTop (66);
        const int w = row.getWidth() / 5;
        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (w));
    };

    knobRow (mixLabel, { &level, &pan, &tune, &overhead, &room });
    knobRow (eqLabel, { &eqLow, &eqMid, &eqFreq, &eqHigh });
    knobRow (dynamicsLabel, { &comp, &attack, &sustain, &drive });

    area.removeFromTop (10);
    separateOut.setBounds (area.removeFromTop (26));
}

//==============================================================================
KitPage::KitPage (DrumWoodysProcessor& p) : processor (p), map (p)
{
    hint.setText (utf8 ("Clique numa peça para ouvir e editar. Mais perto do centro toca mais forte."), juce::dontSendNotification);
    styleHint (hint);
    addAndMakeVisible (hint);

    map.onSelect = [this] (int slot) { select (slot); };
    addAndMakeVisible (map);

    for (int s = 0; s < numSlots; ++s)
    {
        panels[(size_t) s] = std::make_unique<SlotPanel> (processor, s);
        addChildComponent (*panels[(size_t) s]);
    }

    select (kick);
}

void KitPage::select (int slot)
{
    selected = slot;
    map.setSelected (slot);
    for (int s = 0; s < numSlots; ++s)
        panels[(size_t) s]->setVisible (s == slot);
}

void KitPage::refresh()
{
    for (auto& panel : panels)
        panel->refresh();
    map.repaint();
}

void KitPage::flashSlot (int slot)
{
    map.flash (slot);
}

void KitPage::tick()
{
    map.tick();
    panels[(size_t) selected]->decay();
}

void KitPage::paint (juce::Graphics& g)
{
    fillPanel (g, mapArea);
}

void KitPage::resized()
{
    auto area = getLocalBounds();
    auto panelArea = area.removeFromRight (440);
    area.removeFromRight (12);
    mapArea = area;

    for (auto& panel : panels)
        panel->setBounds (panelArea);

    auto inner = mapArea.reduced (16, 12);
    hint.setBounds (inner.removeFromTop (22));
    map.setBounds (inner);
}

//==============================================================================
MixerPage::Strip::Strip (DrumWoodysProcessor& p, const juce::String& n, juce::Colour c,
                         const juce::String& levelId, const juce::String& muteId, const juce::String& soloId, const juce::String& panId)
    : colour (c)
{
    name.setText (n, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    name.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (name);

    addAndMakeVisible (meter);

    level.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
    level.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    level.setColour (juce::Slider::trackColourId, c);
    level.setTextValueSuffix ("");
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.parameters, levelId, level);
    level.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (level);

    mute.setClickingTogglesState (true);
    mute.setColour (juce::TextButton::buttonOnColourId, colours::mute);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.parameters, muteId, mute);
    addAndMakeVisible (mute);

    if (soloId.isNotEmpty())
    {
        solo.setClickingTogglesState (true);
        solo.setColour (juce::TextButton::buttonOnColourId, colours::solo);
        soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.parameters, soloId, solo);
        addAndMakeVisible (solo);
    }

    if (panId.isNotEmpty())
    {
        pan = std::make_unique<LabeledKnob> (p.parameters, panId, "Pan", c);
        addAndMakeVisible (*pan);
    }
}

void MixerPage::Strip::paint (juce::Graphics& g)
{
    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) getWidth() * 0.25f, 6.0f, (float) getWidth() * 0.5f, 3.0f), 1.5f);
}

void MixerPage::Strip::resized()
{
    auto area = getLocalBounds().reduced (3, 12);
    name.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    auto buttons = area.removeFromTop (22);
    if (soloAttachment != nullptr)
    {
        mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
        solo.setBounds (buttons.reduced (1, 0));
    }
    else
    {
        mute.setBounds (buttons.withSizeKeepingCentre (juce::jmin (26, buttons.getWidth()), 22));
    }

    area.removeFromTop (6);
    if (pan != nullptr)
        pan->setBounds (area.removeFromTop (52));
    else
        area.removeFromTop (52);
    area.removeFromTop (6);

    auto faderArea = area;
    auto meterArea = faderArea.removeFromLeft (8).withTrimmedBottom (20);
    meter.setBounds (meterArea);
    level.setBounds (faderArea);
}

MixerPage::MixerPage (DrumWoodysProcessor& p)
    : processor (p),
      masterAttachment (p.parameters, "master", master),
      glue      (p.parameters, "glue", "Cola (comp.)"),
      roomSize  (p.parameters, "roomSize", "Tamanho sala", colours::accent2),
      roomComp  (p.parameters, "roomComp", "Comp. sala", colours::accent2),
      humanize  (p.parameters, "humanize", "Humanizar"),
      variation (p.parameters, "variation", utf8 ("Variação"))
{
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& def = slotDefs()[(size_t) s];
        strips.push_back (std::make_unique<Strip> (p, def.name, slotColour (s),
                                                   DrumWoodysProcessor::paramId (s, "level"), DrumWoodysProcessor::paramId (s, "mute"),
                                                   DrumWoodysProcessor::paramId (s, "solo"), DrumWoodysProcessor::paramId (s, "pan")));
    }

    strips.push_back (std::make_unique<Strip> (p, "Overhead", colours::accent2, "ohLevel", "ohMute", "", ""));
    strips.push_back (std::make_unique<Strip> (p, "Room", colours::accent2, "roomLevel", "roomMute", "", ""));

    for (auto& strip : strips)
        addAndMakeVisible (*strip);

    masterLabel.setText ("MASTER", juce::dontSendNotification);
    masterLabel.setJustificationType (juce::Justification::centred);
    masterLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (masterLabel);
    addAndMakeVisible (masterMeter);

    master.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    master.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    master.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (master);

    glue.setTooltip (utf8 ("Compressor suave na mistura toda, para a bateria soar coesa"));
    roomComp.setTooltip (utf8 ("Comprime o som da sala, deixando o ambiente mais presente"));
    humanize.setTooltip (utf8 ("Varia levemente volume e afinação de cada batida"));
    variation.setTooltip (utf8 ("Evita que batidas repetidas soem idênticas (efeito metralhadora)"));

    for (auto* k : { &glue, &roomSize, &roomComp, &humanize, &variation })
        addAndMakeVisible (*k);
}

void MixerPage::updateMeters()
{
    auto& meters = processor.engine.meters;
    for (int s = 0; s < numSlots; ++s)
        strips[(size_t) s]->meter.setLevel (meters.slots[(size_t) s].load (std::memory_order_relaxed));
    strips[(size_t) numSlots]->meter.setLevel (meters.overhead.load (std::memory_order_relaxed));
    strips[(size_t) numSlots + 1]->meter.setLevel (meters.room.load (std::memory_order_relaxed));
    masterMeter.setLevel (meters.master.load (std::memory_order_relaxed));
}

void MixerPage::paint (juce::Graphics& g)
{
    fillPanel (g, stripsArea);
    fillPanel (g, masterArea);

    // Separate the mic buses from the pieces
    const int stripWidth = stripsArea.getWidth() / (int) strips.size();
    const float x = (float) (stripsArea.getX() + numSlots * stripWidth);
    g.setColour (colours::line);
    g.drawLine (x, (float) stripsArea.getY() + 16.0f, x, (float) stripsArea.getBottom() - 16.0f, 1.0f);
}

void MixerPage::resized()
{
    auto area = getLocalBounds();
    masterArea = area.removeFromRight (210);
    area.removeFromRight (12);
    stripsArea = area;

    const int stripWidth = stripsArea.getWidth() / (int) strips.size();
    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (stripsArea.getX() + (int) i * stripWidth + 4, stripsArea.getY(), stripWidth - 2, stripsArea.getHeight());

    auto m = masterArea.reduced (14, 14);
    masterLabel.setBounds (m.removeFromTop (20));
    m.removeFromTop (8);

    auto faders = m.removeFromLeft (64);
    masterMeter.setBounds (faders.removeFromLeft (10).withTrimmedBottom (20));
    master.setBounds (faders);

    m.removeFromLeft (6);
    const int knobHeight = 70;
    for (auto* k : { &glue, &roomSize, &roomComp, &humanize, &variation })
    {
        k->setBounds (m.removeFromTop (knobHeight));
        m.removeFromTop (6);
    }
}
} // namespace woody
