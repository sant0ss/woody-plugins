#include "Pages.h"
#include "PageHelpers.h"

namespace woody
{
using namespace helpers;

namespace
{
juce::String complexityName (int level)
{
    return level == 0 ? juce::String ("Simples") : level == 1 ? utf8 ("Médio") : juce::String ("Complexo");
}

juce::String describe (const Pattern& p)
{
    juce::String text = juce::String (p.bars) + (p.bars > 1 ? " compassos" : " compasso");
    if (p.stepsPerBar % 3 == 0)
        text << utf8 (" · tercinas");
    text << utf8 (" · ") << complexityName (complexityOf (p));
    const auto tempo = suggestedTempo (p.genre);
    if (tempo.isNotEmpty())
        text << utf8 (" · sugerido ") << tempo;
    return text;
}

void setPlayButton (juce::TextButton& button, bool playing, const juce::String& idleText = "Tocar")
{
    button.setButtonText (playing ? "Parar" : idleText);
    button.setToggleState (playing, juce::dontSendNotification);
}
} // namespace

//==============================================================================
GroovesPage::GroovesPage (DrumWoodysProcessor& p)
    : processor (p),
      swing (p.parameters, "swing", "Swing", colours::accent2),
      timing (p.parameters, "timing", "Tempo humano", colours::accent2)
{
    genres = builtInGenres();
    genres.add (utf8 ("Meus padrões"));
    userPatterns = UserPatterns::load();

    genreModel.numRows = [this] { return genres.size(); };
    genreModel.text = [this] (int row) { return genres[row]; };
    genreModel.onSelect = [this] (int row) { selectGenre (row); };

    patternModel.numRows = [this] { return (int) patterns.size(); };
    patternModel.text = [this] (int row) { return patterns[(size_t) row].name; };
    patternModel.detail = [this] (int row)
    {
        const auto& pat = patterns[(size_t) row];
        return (pat.bars > 1 ? juce::String (pat.bars) + "c  " : juce::String()) + complexityName (complexityOf (pat));
    };
    patternModel.onSelect = [this] (int row) { selectPattern (row); };
    patternModel.onDoubleClick = [this] (int row)
    {
        selectPattern (row);
        processor.play (sequence(), sourceId);
    };

    for (auto* list : { &genreList, &patternList })
    {
        list->setRowHeight (32);
        addAndMakeVisible (*list);
    }
    genreList.setModel (&genreModel);
    patternList.setModel (&patternModel);

    complexityFilter.addItem ("Todas as complexidades", 1);
    complexityFilter.addItem ("Simples", 2);
    complexityFilter.addItem (utf8 ("Médio"), 3);
    complexityFilter.addItem ("Complexo", 4);
    complexityFilter.setSelectedId (1, juce::dontSendNotification);
    complexityFilter.onChange = [this] { refreshPatternList(); };
    addAndMakeVisible (complexityFilter);

    styleTitle (title, 22.0f);
    addAndMakeVisible (title);
    styleHint (info);
    addAndMakeVisible (info);

    playButton.onClick = [this] { togglePlay(); };
    varyButton.onClick = [this]
    {
        if (! hasSelection)
            return;
        showPattern (makeVariation (base, seed++));
    };
    fillButton.onClick = [this]
    {
        if (! hasSelection)
            return;
        showPattern (makeFill (base, seed++));
    };
    editButton.onClick = [this] { if (hasSelection && onEdit != nullptr) onEdit (selected); };
    addButton.onClick = [this] { if (hasSelection && onAddToSong != nullptr) onAddToSong (selected); };
    deleteButton.onClick = [this]
    {
        const auto it = std::find_if (userPatterns.begin(), userPatterns.end(), [this] (const Pattern& u) { return u.name == base.name; });
        if (! isUserGenre() || it == userPatterns.end())
            return;

        userPatterns.erase (it);
        UserPatterns::save (userPatterns);
        selectGenre (genres.size() - 1);
    };

    varyButton.setTooltip (utf8 ("Cria uma nova versão deste groove: notas fantasma, bumbos extras, chimbal aberto..."));
    fillButton.setTooltip (utf8 ("Gera uma virada no fim deste groove. Clique de novo para outra"));
    editButton.setTooltip (utf8 ("Abre no Editor para você mudar as notas"));
    addButton.setTooltip (utf8 ("Coloca no fim da sua música (aba Música)"));
    swing.setTooltip (utf8 ("Atrasa as notas fora do tempo para dar balanço"));
    timing.setTooltip (utf8 ("Adianta e atrasa levemente as notas, como um baterista de verdade"));

    playButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.25f));
    for (auto* b : { &playButton, &varyButton, &fillButton, &editButton, &addButton, &deleteButton })
        addAndMakeVisible (*b);

    addAndMakeVisible (swing);
    addAndMakeVisible (timing);

    drag.getSequence = [this] { return sequence(); };
    drag.getName = [this] { return selected.genre + " - " + selected.name; };
    drag.getTiming = [this] { return processor.getTiming(); };
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

void GroovesPage::selectGenre (int)
{
    deleteButton.setVisible (isUserGenre());
    refreshPatternList();
}

void GroovesPage::refreshPatternList()
{
    const int index = genreList.getSelectedRow();
    const int wanted = complexityFilter.getSelectedId() - 2; // -1 = all

    patterns.clear();
    const auto& source = index == genres.size() - 1 ? userPatterns : builtInPatterns();
    for (const auto& pat : source)
        if ((index == genres.size() - 1 || pat.genre == genres[index]) && (wanted < 0 || complexityOf (pat) == wanted))
            patterns.push_back (pat);

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
        title.setText (index == genres.size() - 1 ? utf8 ("Nenhum padrão salvo ainda") : utf8 ("Nada com esse filtro"), juce::dontSendNotification);
        info.setText (index == genres.size() - 1 ? utf8 ("Crie um groove na aba Editor e clique em \"Salvar em Meus padrões\".")
                                                 : utf8 ("Escolha outra complexidade."),
                      juce::dontSendNotification);
        preview.setPattern (nullptr, false);
    }
}

void GroovesPage::selectPattern (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) patterns.size()))
        return;

    base = patterns[(size_t) index];
    showPattern (base);
}

void GroovesPage::showPattern (const Pattern& pattern)
{
    selected = pattern;
    hasSelection = true;

    title.setText (selected.genre + " - " + selected.name, juce::dontSendNotification);
    info.setText (describe (selected), juce::dontSendNotification);

    preview.setPattern (&selected, false);
    resized();

    // Switching while playing keeps the groove running in time
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
    setPlayButton (playButton, processor.isPlaying() && processor.getPlayingSource() == sourceId);
    preview.setPlayStep (hasSelection ? playStepFor (processor, sourceId, selected) : -1);
}

void GroovesPage::paint (juce::Graphics& g)
{
    fillPanel (g, genreList.getBounds().expanded (0, 6));
    fillPanel (g, patternList.getBounds().getUnion (complexityFilter.getBounds()).expanded (6, 6));
    fillPanel (g, detailArea);
}

void GroovesPage::resized()
{
    auto area = getLocalBounds();
    genreList.setBounds (area.removeFromLeft (170).reduced (0, 6));
    area.removeFromLeft (12);

    auto middle = area.removeFromLeft (260).reduced (6, 6);
    complexityFilter.setBounds (middle.removeFromTop (30));
    middle.removeFromTop (6);
    patternList.setBounds (middle);
    area.removeFromLeft (12);
    detailArea = area;

    auto d = area.reduced (20, 18);
    title.setBounds (d.removeFromTop (30));
    info.setBounds (d.removeFromTop (20));
    d.removeFromTop (14);

    auto controls = d.removeFromTop (70);
    timing.setBounds (controls.removeFromRight (78));
    swing.setBounds (controls.removeFromRight (70));

    auto row1 = controls.removeFromTop (32);
    playButton.setBounds (row1.removeFromLeft (90));
    row1.removeFromLeft (8);
    varyButton.setBounds (row1.removeFromLeft (84));
    row1.removeFromLeft (8);
    fillButton.setBounds (row1.removeFromLeft (84));

    controls.removeFromTop (6);
    auto row2 = controls.removeFromTop (32);
    editButton.setBounds (row2.removeFromLeft (90));
    row2.removeFromLeft (8);
    addButton.setBounds (row2.removeFromLeft (176));
    row2.removeFromLeft (8);
    deleteButton.setBounds (row2.removeFromLeft (84));

    d.removeFromTop (12);
    drag.setBounds (d.removeFromTop (46));
    d.removeFromTop (16);
    preview.setBounds (d.withHeight (juce::jmin (d.getHeight(), preview.getIdealHeight())));
}

//==============================================================================
EditorPage::EditorPage (DrumWoodysProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);

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
        pushUndo();
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
        pushUndo();
        processor.editPattern.resize (processor.editPattern.bars, gridBox.getSelectedId());
        grid.refresh();
        patternChanged();
    };
    addAndMakeVisible (gridBox);

    playButton.onClick = [this] { togglePlay(); };
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    varyButton.onClick = [this] { replacePattern (makeVariation (processor.editPattern, seed++)); };
    fillButton.onClick = [this] { replacePattern (makeFill (processor.editPattern, seed++)); };
    clearButton.onClick = [this]
    {
        pushUndo();
        processor.editPattern.hits.clear();
        grid.refresh();
        patternChanged();
    };
    saveButton.onClick = [this] { saveToUserPatterns(); };
    addButton.onClick = [this] { if (onAddToSong != nullptr) onAddToSong (processor.editPattern); };

    undoButton.setTooltip ("Desfazer (Ctrl+Z)");
    redoButton.setTooltip ("Refazer (Ctrl+Y)");
    varyButton.setTooltip (utf8 ("Acrescenta variações ao groove: notas fantasma, bumbos, chimbal aberto"));
    fillButton.setTooltip (utf8 ("Troca o fim do groove por uma virada"));
    saveButton.setTooltip (utf8 ("Guarda na lista Meus padrões (aba Grooves), disponível em qualquer projeto"));
    addButton.setTooltip (utf8 ("Coloca no fim da sua música (aba Música)"));

    playButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.25f));
    for (auto* b : { &playButton, &undoButton, &redoButton, &varyButton, &fillButton, &clearButton, &saveButton, &addButton })
        addAndMakeVisible (*b);

    drag.getSequence = [this] { return sequence(); };
    drag.getName = [this] { return processor.editPattern.name; };
    drag.getTiming = [this] { return processor.getTiming(); };
    addAndMakeVisible (drag);

    help.setText (utf8 ("Clique para pôr ou tirar uma nota, arraste para pintar várias. Botão direito muda a força "
                        "(fraca, média, forte, acento). Clique no nome da peça para ouvir. Espaço toca/para."),
                  juce::dontSendNotification);
    styleHint (help);
    addAndMakeVisible (help);

    grid.setPattern (&processor.editPattern, true);
    grid.onEditStart = [this] { pushUndo(); };
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
    undoButton.setEnabled (! undoStack.empty());
    redoButton.setEnabled (! redoStack.empty());
}

void EditorPage::pushUndo()
{
    undoStack.push_back (processor.editPattern);
    if (undoStack.size() > 100)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
    undoButton.setEnabled (true);
    redoButton.setEnabled (false);
}

void EditorPage::undo()
{
    if (undoStack.empty())
        return;

    redoStack.push_back (processor.editPattern);
    processor.editPattern = undoStack.back();
    undoStack.pop_back();

    refreshControls();
    grid.refresh();
    resized();
    patternChanged();
}

void EditorPage::redo()
{
    if (redoStack.empty())
        return;

    undoStack.push_back (processor.editPattern);
    processor.editPattern = redoStack.back();
    redoStack.pop_back();

    refreshControls();
    grid.refresh();
    resized();
    patternChanged();
}

void EditorPage::replacePattern (const Pattern& pattern)
{
    pushUndo();
    const auto name = processor.editPattern.name;
    processor.editPattern = pattern;
    processor.editPattern.name = name;
    refreshControls();
    grid.refresh();
    patternChanged();
}

void EditorPage::loadPattern (const Pattern& pattern)
{
    pushUndo();
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

bool EditorPage::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        mods.isShiftDown() ? redo() : undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'Y')
    {
        redo();
        return true;
    }
    if (key == juce::KeyPress::spaceKey && ! nameEditor.hasKeyboardFocus (false))
    {
        togglePlay();
        return true;
    }
    return false;
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
}

void EditorPage::updatePlayState()
{
    setPlayButton (playButton, processor.isPlaying() && processor.getPlayingSource() == sourceId);
    grid.setPlayStep (playStepFor (processor, sourceId, processor.editPattern));
}

void EditorPage::paint (juce::Graphics& g)
{
    fillPanel (g, viewport.getBounds().expanded (8));
}

void EditorPage::resized()
{
    auto area = getLocalBounds();

    auto row = area.removeFromTop (32);
    nameLabel.setBounds (row.removeFromLeft (44));
    nameEditor.setBounds (row.removeFromLeft (190));
    row.removeFromLeft (16);
    barsLabel.setBounds (row.removeFromLeft (78));
    barsBox.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (16);
    gridLabel.setBounds (row.removeFromLeft (46));
    gridBox.setBounds (row.removeFromLeft (124));
    row.removeFromLeft (16);
    playButton.setBounds (row.removeFromLeft (84));
    row.removeFromLeft (6);
    undoButton.setBounds (row.removeFromLeft (92));
    row.removeFromLeft (6);
    redoButton.setBounds (row.removeFromLeft (88));

    area.removeFromTop (10);
    auto row2 = area.removeFromTop (44);
    varyButton.setBounds (row2.removeFromLeft (84).withSizeKeepingCentre (84, 32));
    row2.removeFromLeft (6);
    fillButton.setBounds (row2.removeFromLeft (84).withSizeKeepingCentre (84, 32));
    row2.removeFromLeft (6);
    clearButton.setBounds (row2.removeFromLeft (84).withSizeKeepingCentre (84, 32));
    row2.removeFromLeft (16);
    saveButton.setBounds (row2.removeFromLeft (196).withSizeKeepingCentre (196, 32));
    row2.removeFromLeft (6);
    addButton.setBounds (row2.removeFromLeft (176).withSizeKeepingCentre (176, 32));
    row2.removeFromLeft (16);
    drag.setBounds (row2);

    area.removeFromTop (8);
    help.setBounds (area.removeFromTop (20));
    area.removeFromTop (14);

    viewport.setBounds (area.reduced (8));
    grid.setSize (viewport.getMaximumVisibleWidth(), grid.getIdealHeight());
}

//==============================================================================
SongPage::PartBlock::PartBlock (SongPage& p, int i) : page (p), index (i)
{
    const auto& part = page.processor.song[(size_t) index];

    name.setText (part.pattern.name, juce::dontSendNotification);
    name.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    name.setJustificationType (juce::Justification::centred);
    name.setMinimumHorizontalScale (0.6f);
    genre.setText (part.pattern.genre, juce::dontSendNotification);
    genre.setJustificationType (juce::Justification::centred);
    styleHint (genre);
    repeats.setText ("x" + juce::String (part.repeats), juce::dontSendNotification);
    repeats.setJustificationType (juce::Justification::centred);
    repeats.setFont (juce::FontOptions (22.0f, juce::Font::bold));
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
    {
        b->setColour (juce::TextButton::buttonColourId, colours::line);
        addAndMakeVisible (*b);
    }

    drag.setTooltip (utf8 ("Arraste só esta parte para o Ableton"));
    drag.getSequence = [this]
    {
        const auto& song = page.processor.song;
        if (! juce::isPositiveAndBelow (index, (int) song.size()))
            return Sequence {};
        return makeSequence (std::vector<SongPart> { song[(size_t) index] }, swingValue (page.processor));
    };
    drag.getName = [this] { return page.processor.song[(size_t) index].pattern.name; };
    drag.getTiming = [this] { return page.processor.getTiming(); };
    addAndMakeVisible (drag);
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
    const auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    g.setColour (highlighted ? colours::accent.withAlpha (0.18f) : colours::panelLight);
    g.fillRoundedRectangle (bounds, 12.0f);
    if (highlighted)
    {
        g.setColour (colours::accent);
        g.drawRoundedRectangle (bounds, 12.0f, 1.5f);
    }
}

void SongPage::PartBlock::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto top = area.removeFromTop (22);
    remove.setBounds (top.removeFromRight (22));
    name.setBounds (area.removeFromTop (20));
    genre.setBounds (area.removeFromTop (18));

    drag.setBounds (area.removeFromBottom (26));
    area.removeFromBottom (6);
    auto bottom = area.removeFromBottom (24);
    left.setBounds (bottom.removeFromLeft (28));
    right.setBounds (bottom.removeFromRight (28));

    auto middle = area.reduced (0, 2);
    minus.setBounds (middle.removeFromLeft (28).withSizeKeepingCentre (28, 26));
    plus.setBounds (middle.removeFromRight (28).withSizeKeepingCentre (28, 26));
    repeats.setBounds (middle);
}

SongPage::SongPage (DrumWoodysProcessor& p) : processor (p)
{
    playButton.onClick = [this] { togglePlay(); };
    playButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.25f));
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
    drag.getTiming = [this] { return processor.getTiming(); };
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
                                          : juce::String ((int) processor.song.size()) + (processor.song.size() == 1 ? " parte" : " partes")
                                                + utf8 (" · ") + juce::String (bars) + (bars == 1 ? " compasso" : " compassos"),
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
    setPlayButton (playButton, playing, utf8 ("Tocar música"));

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
    fillPanel (g, viewport.getBounds().expanded (8));
}

void SongPage::resized()
{
    auto area = getLocalBounds();

    auto row = area.removeFromTop (46);
    playButton.setBounds (row.removeFromLeft (140).withSizeKeepingCentre (140, 32));
    row.removeFromLeft (8);
    clearButton.setBounds (row.removeFromLeft (90).withSizeKeepingCentre (90, 32));
    row.removeFromLeft (16);
    drag.setBounds (row.removeFromLeft (400));
    row.removeFromLeft (16);
    total.setBounds (row);

    area.removeFromTop (20);
    viewport.setBounds (area.removeFromTop (juce::jmin (area.getHeight(), 220)).reduced (8));
    empty.setBounds (viewport.getBounds().reduced (40, 20));

    constexpr int blockWidth = 180;
    const int height = viewport.getMaximumVisibleHeight();
    strip.setSize (juce::jmax (viewport.getWidth(), (int) blocks.size() * blockWidth + 8), height);
    for (size_t i = 0; i < blocks.size(); ++i)
        blocks[i]->setBounds (4 + (int) i * blockWidth, 4, blockWidth, height - 8);
}
} // namespace woody
