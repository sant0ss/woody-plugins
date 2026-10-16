#include "Pages.h"

namespace woody
{
namespace
{
// Plays a note once from the UI.
void audition (DrumWoodysProcessor& processor, int note, float velocity)
{
    processor.keyboardState.noteOn (1, note, velocity);
    processor.keyboardState.noteOff (1, note, 0.0f);
}

juce::String noteName (int note)
{
    // Ableton names middle C (60) as C3
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
}

float swingValue (DrumWoodysProcessor& processor)
{
    return processor.parameters.getRawParameterValue ("swing")->load();
}

int playStepFor (DrumWoodysProcessor& processor, const char* source, const Pattern& pattern)
{
    if (! processor.isPlaying() || processor.getPlayingSource() != source)
        return -1;

    const double position = processor.getPlayPosition();
    if (position < 0.0)
        return -1;

    return (int) (position * pattern.stepsPerBar / 4.0);
}

void styleTitle (juce::Label& label, float size)
{
    label.setFont (juce::FontOptions (size, juce::Font::bold));
    label.setColour (juce::Label::textColourId, colours::text);
}

void styleHint (juce::Label& label)
{
    label.setFont (juce::FontOptions (13.0f));
    label.setColour (juce::Label::textColourId, colours::textDim);
}
} // namespace

//==============================================================================
void SimpleListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (2.0f, 1.0f), 4.0f);
    }

    g.setColour (selected ? colours::text : colours::text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (14.0f));
    if (text != nullptr)
        g.drawText (text (row), 10, 0, width - 14, height, juce::Justification::centredLeft);
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

//==============================================================================
ArticulationPad::ArticulationPad (DrumWoodysProcessor& p, int slot, int articulation)
    : processor (p)
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
        flashLevel = juce::jmax (0.0f, flashLevel - 0.12f);
        repaint();
    }
}

void ArticulationPad::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.5f);

    g.setColour (colours::panelLight.interpolatedWith (colours::accent, flashLevel * 0.8f));
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (label, bounds.reduced (8.0f, 0.0f), juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (noteName (note) + " " + juce::String (note), bounds.reduced (8.0f, 0.0f), juce::Justification::centredRight);
}

void ArticulationPad::mouseDown (const juce::MouseEvent& e)
{
    const float height = (float) juce::jmax (1, getHeight());
    const float velocity = juce::jlimit (0.2f, 1.0f, 1.0f - (float) e.y / height * 0.8f);
    processor.keyboardState.noteOn (1, note, velocity);
    flash();
}

void ArticulationPad::mouseUp (const juce::MouseEvent&)
{
    processor.keyboardState.noteOff (1, note, 0.0f);
}

//==============================================================================
SlotCard::SlotCard (DrumWoodysProcessor& p, int s)
    : processor (p), slot (s),
      tune (p.parameters, juce::String (slotDefs()[(size_t) s].id) + "_tune", utf8 ("Afinação"))
{
    const auto& def = slotDefs()[(size_t) slot];

    title.setText (def.name, juce::dontSendNotification);
    styleTitle (title, 15.0f);
    addAndMakeVisible (title);

    piece.setTooltip (utf8 ("Escolha a peça para esta posição"));
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

    addAndMakeVisible (tune);
    refresh();
}

void SlotCard::refresh()
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
}

void SlotCard::flash()
{
    flashLevel = 1.0f;
    repaint();
}

void SlotCard::decay()
{
    for (auto& pad : pads)
        pad->decay();

    if (flashLevel > 0.0f)
    {
        flashLevel = juce::jmax (0.0f, flashLevel - 0.1f);
        repaint();
    }
}

void SlotCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (colours::panelLight.interpolatedWith (colours::accent, flashLevel));
    g.drawRoundedRectangle (bounds, 8.0f, 1.5f);
}

void SlotCard::resized()
{
    auto area = getLocalBounds().reduced (10);
    title.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    piece.setBounds (area.removeFromTop (26));
    area.removeFromTop (8);

    tune.setBounds (area.removeFromBottom (64).withSizeKeepingCentre (70, 64));
    area.removeFromBottom (4);

    for (auto& pad : pads)
    {
        pad->setBounds (area.removeFromTop (30));
        area.removeFromTop (4);
    }
}

//==============================================================================
KitPage::KitPage (DrumWoodysProcessor& p) : processor (p)
{
    hint.setText (utf8 ("Escolha a peça de cada posição em qualquer kit. Clique nos pads para ouvir: mais alto no pad toca mais forte. "
                        "Cada pad mostra a nota MIDI que ele usa."),
                  juce::dontSendNotification);
    styleHint (hint);
    addAndMakeVisible (hint);

    resetButton.setTooltip (utf8 ("Volta para o kit inicial"));
    resetButton.onClick = [this] { processor.resetKitToDefaults(); };
    addAndMakeVisible (resetButton);

    for (int s = 0; s < numSlots; ++s)
    {
        cards[(size_t) s] = std::make_unique<SlotCard> (processor, s);
        addAndMakeVisible (*cards[(size_t) s]);
    }
}

void KitPage::refresh()
{
    for (auto& card : cards)
        card->refresh();
}

void KitPage::decay()
{
    for (auto& card : cards)
        card->decay();
}

void KitPage::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    auto top = area.removeFromTop (28);
    resetButton.setBounds (top.removeFromRight (110));
    hint.setBounds (top);
    area.removeFromTop (6);

    constexpr int columns = 7;
    const int cardWidth = area.getWidth() / columns;
    const int cardHeight = area.getHeight() / 2;

    for (int s = 0; s < numSlots; ++s)
        cards[(size_t) s]->setBounds (area.getX() + (s % columns) * cardWidth, area.getY() + (s / columns) * cardHeight, cardWidth, cardHeight);
}

//==============================================================================
MixerPage::Strip::Strip (DrumWoodysProcessor& p, int slot)
    : pan (p.parameters, juce::String (slotDefs()[(size_t) slot].id) + "_pan", "Pan"),
      room (p.parameters, juce::String (slotDefs()[(size_t) slot].id) + "_room", "Sala"),
      levelAttachment (p.parameters, juce::String (slotDefs()[(size_t) slot].id) + "_level", level),
      muteAttachment (p.parameters, juce::String (slotDefs()[(size_t) slot].id) + "_mute", mute),
      soloAttachment (p.parameters, juce::String (slotDefs()[(size_t) slot].id) + "_solo", solo)
{
    name.setText (slotDefs()[(size_t) slot].name, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (name);

    mute.setClickingTogglesState (true);
    mute.setTooltip ("Mudo");
    addAndMakeVisible (mute);

    solo.setClickingTogglesState (true);
    solo.setColour (juce::TextButton::buttonOnColourId, colours::accent.darker (0.2f));
    solo.setTooltip ("Solo");
    addAndMakeVisible (solo);

    level.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    level.setTextValueSuffix (" dB");
    addAndMakeVisible (level);

    addAndMakeVisible (pan);
    addAndMakeVisible (room);
}

void MixerPage::Strip::resized()
{
    auto area = getLocalBounds().reduced (4, 8);
    name.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);

    auto buttons = area.removeFromTop (24);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
    solo.setBounds (buttons.reduced (2, 0));
    area.removeFromTop (6);

    room.setBounds (area.removeFromBottom (58));
    pan.setBounds (area.removeFromBottom (58));
    area.removeFromBottom (4);
    level.setBounds (area);
}

MixerPage::MixerPage (DrumWoodysProcessor& p)
    : masterAttachment (p.parameters, "master", master),
      ambience (p.parameters, "ambience", utf8 ("Ambiência")),
      roomSize (p.parameters, "roomSize", "Tamanho da sala"),
      humanize (p.parameters, "humanize", "Humanizar")
{
    for (int s = 0; s < numSlots; ++s)
    {
        strips[(size_t) s] = std::make_unique<Strip> (p, s);
        addAndMakeVisible (*strips[(size_t) s]);
    }

    masterLabel.setText ("Master", juce::dontSendNotification);
    masterLabel.setJustificationType (juce::Justification::centred);
    masterLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (masterLabel);

    master.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    master.setTextValueSuffix (" dB");
    addAndMakeVisible (master);

    ambience.setTooltip (utf8 ("Quanto do som da sala volta para a mistura"));
    humanize.setTooltip (utf8 ("Varia levemente volume e afinação de cada batida, como um baterista de verdade"));
    addAndMakeVisible (ambience);
    addAndMakeVisible (roomSize);
    addAndMakeVisible (humanize);
}

void MixerPage::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (stripsArea.toFloat(), 10.0f);
    g.fillRoundedRectangle (masterArea.toFloat(), 10.0f);

    g.setColour (colours::background);
    const int stripWidth = stripsArea.getWidth() / numSlots;
    for (int s = 1; s < numSlots; ++s)
    {
        const float x = (float) (stripsArea.getX() + s * stripWidth);
        g.drawLine (x, (float) stripsArea.getY() + 12.0f, x, (float) stripsArea.getBottom() - 12.0f, 1.0f);
    }
}

void MixerPage::resized()
{
    auto area = getLocalBounds().reduced (12, 10);
    masterArea = area.removeFromRight (170);
    area.removeFromRight (12);
    stripsArea = area;

    const int stripWidth = stripsArea.getWidth() / numSlots;
    for (int s = 0; s < numSlots; ++s)
        strips[(size_t) s]->setBounds (stripsArea.getX() + s * stripWidth, stripsArea.getY(), stripWidth, stripsArea.getHeight());

    auto m = masterArea.reduced (10);
    masterLabel.setBounds (m.removeFromTop (20));
    m.removeFromTop (6);
    humanize.setBounds (m.removeFromBottom (72));
    auto upper = m.removeFromBottom (72);
    ambience.setBounds (upper.removeFromLeft (upper.getWidth() / 2));
    roomSize.setBounds (upper);
    master.setBounds (m);
}

//==============================================================================
GroovesPage::GroovesPage (DrumWoodysProcessor& p)
    : processor (p), swing (p.parameters, "swing", "Swing")
{
    genres = builtInGenres();
    genres.add (utf8 ("Meus padrões"));
    userPatterns = UserPatterns::load();

    genreModel.numRows = [this] { return genres.size(); };
    genreModel.text = [this] (int row) { return genres[row]; };
    genreModel.onSelect = [this] (int row) { selectGenre (row); };

    patternModel.numRows = [this] { return (int) patterns.size(); };
    patternModel.text = [this] (int row)
    {
        const auto& pat = patterns[(size_t) row];
        juce::String text = pat.name;
        if (pat.bars > 1)
            text << "  (" << pat.bars << " compassos)";
        if (pat.stepsPerBar % 3 == 0)
            text << utf8 ("  · tercinas");
        return text;
    };
    patternModel.onSelect = [this] (int row) { selectPattern (row); };
    patternModel.onDoubleClick = [this] (int row)
    {
        selectPattern (row);
        processor.play (sequence(), sourceId);
    };

    for (auto* list : { &genreList, &patternList })
    {
        list->setRowHeight (28);
        list->setOutlineThickness (1);
        addAndMakeVisible (*list);
    }
    genreList.setModel (&genreModel);
    patternList.setModel (&patternModel);

    styleTitle (title, 20.0f);
    addAndMakeVisible (title);
    styleHint (info);
    addAndMakeVisible (info);

    playButton.onClick = [this] { togglePlay(); };
    editButton.onClick = [this] { if (hasSelection && onEdit != nullptr) onEdit (selected); };
    addButton.onClick = [this] { if (hasSelection && onAddToSong != nullptr) onAddToSong (selected); };
    deleteButton.onClick = [this]
    {
        const int row = patternList.getSelectedRow();
        if (! isUserGenre() || ! juce::isPositiveAndBelow (row, (int) userPatterns.size()))
            return;

        userPatterns.erase (userPatterns.begin() + row);
        UserPatterns::save (userPatterns);
        selectGenre (genres.size() - 1);
    };

    editButton.setTooltip (utf8 ("Abre este groove no Editor para você mudar as notas"));
    addButton.setTooltip (utf8 ("Coloca este groove no fim da sua música (aba Música)"));
    swing.setTooltip (utf8 ("Atrasa as notas fora do tempo para dar balanço (shuffle)"));

    for (auto* b : { &playButton, &editButton, &addButton, &deleteButton })
        addAndMakeVisible (*b);

    addAndMakeVisible (swing);

    drag.getSequence = [this] { return sequence(); };
    drag.getName = [this] { return selected.genre + " - " + selected.name; };
    addAndMakeVisible (drag);

    preview.setCompact (true);
    preview.onAudition = [this] (int note, float velocity) { audition (processor, note, velocity); };
    addAndMakeVisible (preview);

    genreList.selectRow (0);
}

bool GroovesPage::isUserGenre() const
{
    return genreList.getSelectedRow() == genres.size() - 1;
}

void GroovesPage::reloadUserPatterns()
{
    userPatterns = UserPatterns::load();
    if (isUserGenre())
        selectGenre (genres.size() - 1);
}

void GroovesPage::selectGenre (int index)
{
    patterns.clear();

    if (index == genres.size() - 1)
        patterns = userPatterns;
    else
        for (const auto& pat : builtInPatterns())
            if (pat.genre == genres[index])
                patterns.push_back (pat);

    deleteButton.setVisible (index == genres.size() - 1);
    patternList.updateContent();
    patternList.repaint();

    if (! patterns.empty())
    {
        patternList.selectRow (0);
        selectPattern (0);
    }
    else
    {
        hasSelection = false;
        title.setText (index == genres.size() - 1 ? utf8 ("Nenhum padrão salvo ainda") : juce::String(), juce::dontSendNotification);
        info.setText (utf8 ("Crie um groove na aba Editor e clique em \"Salvar em Meus padrões\"."), juce::dontSendNotification);
        preview.setPattern (nullptr, false);
    }
}

void GroovesPage::selectPattern (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) patterns.size()))
        return;

    selected = patterns[(size_t) index];
    hasSelection = true;

    title.setText (selected.genre + " - " + selected.name, juce::dontSendNotification);
    info.setText (juce::String (selected.bars) + (selected.bars > 1 ? " compassos" : " compasso")
                      + (selected.stepsPerBar % 3 == 0 ? utf8 (" · tercinas") : juce::String())
                      + utf8 (" · clique duas vezes na lista para tocar"),
                  juce::dontSendNotification);

    preview.setPattern (&selected, false);
    resized();

    // Switching sections while playing keeps the groove running in time
    processor.updateSequence (sequence(), sourceId);
}

Sequence GroovesPage::sequence() const
{
    return hasSelection ? makeSequence (selected, swingValue (processor)) : Sequence {};
}

void GroovesPage::togglePlay()
{
    if (processor.isPlaying() && processor.getPlayingSource() == sourceId)
        processor.stop();
    else if (hasSelection)
        processor.play (sequence(), sourceId);
}

void GroovesPage::updatePlayState()
{
    const bool playing = processor.isPlaying() && processor.getPlayingSource() == sourceId;
    playButton.setButtonText (playing ? "Parar" : "Tocar");
    playButton.setToggleState (playing, juce::dontSendNotification);
    preview.setPlayStep (hasSelection ? playStepFor (processor, sourceId, selected) : -1);
}

void GroovesPage::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (detailArea.toFloat(), 10.0f);
}

void GroovesPage::resized()
{
    auto area = getLocalBounds().reduced (12, 10);
    genreList.setBounds (area.removeFromLeft (170));
    area.removeFromLeft (10);
    patternList.setBounds (area.removeFromLeft (250));
    area.removeFromLeft (10);
    detailArea = area;

    auto d = area.reduced (14);
    title.setBounds (d.removeFromTop (28));
    info.setBounds (d.removeFromTop (20));
    d.removeFromTop (10);

    auto controls = d.removeFromTop (64);
    swing.setBounds (controls.removeFromRight (70));
    auto buttons = controls.removeFromTop (30);
    playButton.setBounds (buttons.removeFromLeft (90));
    buttons.removeFromLeft (8);
    editButton.setBounds (buttons.removeFromLeft (140));
    buttons.removeFromLeft (8);
    addButton.setBounds (buttons.removeFromLeft (160));
    buttons.removeFromLeft (8);
    deleteButton.setBounds (buttons.removeFromLeft (80));

    d.removeFromTop (6);
    drag.setBounds (d.removeFromTop (46));
    d.removeFromTop (12);

    preview.setBounds (d.withHeight (juce::jmin (d.getHeight(), preview.getIdealHeight())));
}

//==============================================================================
EditorPage::EditorPage (DrumWoodysProcessor& p) : processor (p)
{
    nameLabel.setText ("Nome", juce::dontSendNotification);
    barsLabel.setText ("Compassos", juce::dontSendNotification);
    gridLabel.setText ("Grade", juce::dontSendNotification);
    for (auto* l : { &nameLabel, &barsLabel, &gridLabel })
    {
        styleHint (*l);
        addAndMakeVisible (*l);
    }

    nameEditor.onTextChange = [this] { processor.editPattern.name = nameEditor.getText(); };
    addAndMakeVisible (nameEditor);

    barsBox.addItem ("1", 1);
    barsBox.addItem ("2", 2);
    barsBox.addItem ("4", 4);
    barsBox.onChange = [this]
    {
        processor.editPattern.resize (barsBox.getSelectedId(), processor.editPattern.stepsPerBar);
        grid.refresh();
        resized();
        patternChanged();
    };
    addAndMakeVisible (barsBox);

    gridBox.addItem ("1/16", 16);
    gridBox.addItem ("1/8", 8);
    gridBox.addItem ("1/8 tercina", 12);
    gridBox.addItem ("1/16 tercina", 24);
    gridBox.onChange = [this]
    {
        processor.editPattern.resize (processor.editPattern.bars, gridBox.getSelectedId());
        grid.refresh();
        patternChanged();
    };
    addAndMakeVisible (gridBox);

    playButton.onClick = [this] { togglePlay(); };
    clearButton.onClick = [this]
    {
        processor.editPattern.hits.clear();
        grid.refresh();
        patternChanged();
    };
    saveButton.onClick = [this] { saveToUserPatterns(); };
    addButton.onClick = [this] { if (onAddToSong != nullptr) onAddToSong (processor.editPattern); };

    saveButton.setTooltip (utf8 ("Guarda este groove na lista Meus padrões (aba Grooves), disponível em qualquer projeto"));
    addButton.setTooltip (utf8 ("Coloca este groove no fim da sua música (aba Música)"));

    for (auto* b : { &playButton, &clearButton, &saveButton, &addButton })
        addAndMakeVisible (*b);

    drag.getSequence = [this] { return sequence(); };
    drag.getName = [this] { return processor.editPattern.name; };
    addAndMakeVisible (drag);

    help.setText (utf8 ("Clique para pôr ou tirar uma nota, arraste para pintar várias. Botão direito muda a força "
                        "(fraca, média, forte, acento). Clique no nome da peça para ouvir."),
                  juce::dontSendNotification);
    styleHint (help);
    addAndMakeVisible (help);

    grid.setPattern (&processor.editPattern, true);
    grid.onChange = [this] { patternChanged(); };
    grid.onAudition = [this] (int note, float velocity) { audition (processor, note, velocity); };

    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    refreshControls();
}

void EditorPage::refreshControls()
{
    const auto& pat = processor.editPattern;
    nameEditor.setText (pat.name, juce::dontSendNotification);
    barsBox.setSelectedId (pat.bars, juce::dontSendNotification);
    gridBox.setSelectedId (pat.stepsPerBar, juce::dontSendNotification);
}

void EditorPage::loadPattern (const Pattern& pattern)
{
    processor.editPattern = pattern;
    processor.editPattern.genre = utf8 ("Meus padrões");
    if (barsBox.indexOfItemId (processor.editPattern.bars) < 0)
        processor.editPattern.resize (4, processor.editPattern.stepsPerBar);

    refreshControls();
    grid.setPattern (&processor.editPattern, true);
    resized();
    patternChanged();
}

Sequence EditorPage::sequence() const
{
    return makeSequence (processor.editPattern, swingValue (processor));
}

void EditorPage::patternChanged()
{
    processor.updateSequence (sequence(), sourceId);
}

void EditorPage::togglePlay()
{
    if (processor.isPlaying() && processor.getPlayingSource() == sourceId)
        processor.stop();
    else
        processor.play (sequence(), sourceId);
}

void EditorPage::saveToUserPatterns()
{
    auto saved = UserPatterns::load();
    auto pattern = processor.editPattern;
    pattern.genre = utf8 ("Meus padrões");
    if (pattern.name.trim().isEmpty())
        pattern.name = "Meu groove";

    // Same name replaces the old version
    auto existing = std::find_if (saved.begin(), saved.end(), [&] (const Pattern& p) { return p.name == pattern.name; });
    if (existing != saved.end())
        *existing = pattern;
    else
        saved.push_back (pattern);

    UserPatterns::save (saved);

    if (onSaved != nullptr)
        onSaved();

    saveButton.setButtonText ("Salvo!");
    juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<EditorPage> (this)]
                                 {
                                     if (safe != nullptr)
                                         safe->saveButton.setButtonText (utf8 ("Salvar em Meus padrões"));
                                 });
}

void EditorPage::updatePlayState()
{
    const bool playing = processor.isPlaying() && processor.getPlayingSource() == sourceId;
    playButton.setButtonText (playing ? "Parar" : "Tocar");
    playButton.setToggleState (playing, juce::dontSendNotification);
    grid.setPlayStep (playStepFor (processor, sourceId, processor.editPattern));
}

void EditorPage::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (viewport.getBounds().toFloat().expanded (4.0f), 8.0f);
}

void EditorPage::resized()
{
    auto area = getLocalBounds().reduced (12, 10);

    auto row = area.removeFromTop (30);
    nameLabel.setBounds (row.removeFromLeft (44));
    nameEditor.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (14);
    barsLabel.setBounds (row.removeFromLeft (80));
    barsBox.setBounds (row.removeFromLeft (60));
    row.removeFromLeft (14);
    gridLabel.setBounds (row.removeFromLeft (46));
    gridBox.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (14);
    playButton.setBounds (row.removeFromLeft (80));
    row.removeFromLeft (6);
    clearButton.setBounds (row.removeFromLeft (80));

    area.removeFromTop (8);
    auto row2 = area.removeFromTop (44);
    saveButton.setBounds (row2.removeFromLeft (190).withSizeKeepingCentre (190, 30));
    row2.removeFromLeft (8);
    addButton.setBounds (row2.removeFromLeft (170).withSizeKeepingCentre (170, 30));
    row2.removeFromLeft (14);
    drag.setBounds (row2.removeFromLeft (320));

    area.removeFromTop (6);
    help.setBounds (area.removeFromTop (22));
    area.removeFromTop (6);

    viewport.setBounds (area.reduced (4));
    grid.setSize (viewport.getMaximumVisibleWidth(), grid.getIdealHeight());
}

//==============================================================================
SongPage::PartBlock::PartBlock (SongPage& p, int i) : page (p), index (i)
{
    const auto& part = page.processor.song[(size_t) index];

    name.setText (part.pattern.name, juce::dontSendNotification);
    name.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    name.setJustificationType (juce::Justification::centred);
    genre.setText (part.pattern.genre, juce::dontSendNotification);
    genre.setJustificationType (juce::Justification::centred);
    styleHint (genre);
    repeats.setText ("x" + juce::String (part.repeats), juce::dontSendNotification);
    repeats.setJustificationType (juce::Justification::centred);
    repeats.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    repeats.setColour (juce::Label::textColourId, colours::accent);

    for (auto* l : { &name, &genre, &repeats })
    {
        l->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*l);
    }

    auto edit = [this] (std::function<void (std::vector<SongPart>&)> change)
    {
        change (page.processor.song);
        page.changed();
        // Rebuilding deletes this block, so do it after the click finishes
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SongPage> (&page)]
                                         {
                                             if (safe != nullptr)
                                                 safe->rebuild();
                                         });
    };

    minus.onClick = [this, edit] { edit ([this] (auto& song) { auto& r = song[(size_t) index].repeats; r = juce::jmax (1, r - 1); }); };
    plus.onClick  = [this, edit] { edit ([this] (auto& song) { auto& r = song[(size_t) index].repeats; r = juce::jmin (32, r + 1); }); };
    left.onClick  = [this, edit] { edit ([this] (auto& song) { if (index > 0) std::swap (song[(size_t) index], song[(size_t) index - 1]); }); };
    right.onClick = [this, edit] { edit ([this] (auto& song) { if (index + 1 < (int) song.size()) std::swap (song[(size_t) index], song[(size_t) index + 1]); }); };
    remove.onClick = [this, edit] { edit ([this] (auto& song) { song.erase (song.begin() + index); }); };

    minus.setTooltip ("Tocar menos vezes");
    plus.setTooltip ("Tocar mais vezes");
    left.setTooltip ("Mover para a esquerda");
    right.setTooltip ("Mover para a direita");
    remove.setTooltip ("Remover");

    for (auto* b : { &minus, &plus, &left, &right, &remove })
        addAndMakeVisible (*b);
}

void SongPage::PartBlock::setHighlighted (bool shouldHighlight)
{
    if (highlighted != shouldHighlight)
    {
        highlighted = shouldHighlight;
        repaint();
    }
}

void SongPage::PartBlock::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (highlighted ? colours::accent.withAlpha (0.3f) : colours::panelLight);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (highlighted ? colours::accent : colours::panelLight.brighter (0.2f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.5f);
}

void SongPage::PartBlock::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto top = area.removeFromTop (22);
    remove.setBounds (top.removeFromRight (22));
    name.setBounds (area.removeFromTop (20));
    genre.setBounds (area.removeFromTop (18));

    auto bottom = area.removeFromBottom (24);
    left.setBounds (bottom.removeFromLeft (28));
    right.setBounds (bottom.removeFromRight (28));

    auto middle = area.reduced (0, 4);
    minus.setBounds (middle.removeFromLeft (28).withSizeKeepingCentre (28, 26));
    plus.setBounds (middle.removeFromRight (28).withSizeKeepingCentre (28, 26));
    repeats.setBounds (middle);
}

SongPage::SongPage (DrumWoodysProcessor& p) : processor (p)
{
    playButton.onClick = [this] { togglePlay(); };
    clearButton.onClick = [this]
    {
        processor.song.clear();
        changed();
        rebuild();
    };
    addAndMakeVisible (playButton);
    addAndMakeVisible (clearButton);

    drag.getSequence = [this] { return sequence(); };
    drag.getName = [] { return utf8 ("Drum Woodys - Música"); };
    addAndMakeVisible (drag);

    styleHint (total);
    addAndMakeVisible (total);

    empty.setText (utf8 ("Sua música está vazia. Nas abas Grooves ou Editor, clique em \"Adicionar à Música\" para montar a "
                         "sequência: intro, verso, virada, refrão... Depois ajuste quantas vezes cada parte toca."),
                   juce::dontSendNotification);
    empty.setJustificationType (juce::Justification::centred);
    styleHint (empty);
    addAndMakeVisible (empty);

    viewport.setViewedComponent (&strip, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);

    rebuild();
}

void SongPage::addPart (const Pattern& pattern)
{
    processor.song.push_back ({ pattern, 1 });
    changed();
    rebuild();
    viewport.setViewPosition (strip.getWidth(), 0);
}

Sequence SongPage::sequence() const
{
    return makeSequence (processor.song, swingValue (processor));
}

void SongPage::changed()
{
    int bars = 0;
    for (const auto& part : processor.song)
        bars += part.pattern.bars * part.repeats;

    total.setText (processor.song.empty() ? juce::String()
                                          : juce::String ((int) processor.song.size()) + utf8 (" partes · ") + juce::String (bars) + " compassos",
                   juce::dontSendNotification);

    if (processor.song.empty() && processor.getPlayingSource() == sourceId)
        processor.stop();
    else
        processor.updateSequence (sequence(), sourceId);
}

void SongPage::rebuild()
{
    blocks.clear();

    for (int i = 0; i < (int) processor.song.size(); ++i)
    {
        blocks.push_back (std::make_unique<PartBlock> (*this, i));
        strip.addAndMakeVisible (*blocks.back());
    }

    empty.setVisible (processor.song.empty());
    changed();
    resized();
}

void SongPage::togglePlay()
{
    if (processor.isPlaying() && processor.getPlayingSource() == sourceId)
        processor.stop();
    else if (! processor.song.empty())
        processor.play (sequence(), sourceId);
}

void SongPage::updatePlayState()
{
    const bool playing = processor.isPlaying() && processor.getPlayingSource() == sourceId;
    playButton.setButtonText (playing ? "Parar" : utf8 ("Tocar música"));
    playButton.setToggleState (playing, juce::dontSendNotification);

    // Highlight the part under the playhead
    const double position = playing ? processor.getPlayPosition() : -1.0;
    double start = 0.0;
    for (size_t i = 0; i < blocks.size() && i < processor.song.size(); ++i)
    {
        const double length = processor.song[i].pattern.lengthBeats() * processor.song[i].repeats;
        blocks[i]->setHighlighted (position >= start && position < start + length);
        start += length;
    }
}

void SongPage::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (viewport.getBounds().toFloat().expanded (4.0f), 10.0f);
}

void SongPage::resized()
{
    auto area = getLocalBounds().reduced (12, 10);

    auto row = area.removeFromTop (46);
    playButton.setBounds (row.removeFromLeft (130).withSizeKeepingCentre (130, 30));
    row.removeFromLeft (8);
    clearButton.setBounds (row.removeFromLeft (90).withSizeKeepingCentre (90, 30));
    row.removeFromLeft (14);
    drag.setBounds (row.removeFromLeft (380));
    row.removeFromLeft (14);
    total.setBounds (row);

    area.removeFromTop (12);
    viewport.setBounds (area.removeFromTop (juce::jmin (area.getHeight(), 190)).reduced (4));
    empty.setBounds (viewport.getBounds().reduced (40, 20));

    constexpr int blockWidth = 170;
    const int height = viewport.getMaximumVisibleHeight();
    strip.setSize (juce::jmax (viewport.getWidth(), (int) blocks.size() * blockWidth + 8), height);
    for (size_t i = 0; i < blocks.size(); ++i)
        blocks[i]->setBounds (4 + (int) i * blockWidth, 4, blockWidth, height - 8);
}
} // namespace woody
