#include "PluginEditor.h"

using namespace woody;

MainView::MainView (DrumWoodysProcessor& p, juce::LookAndFeel& lnf)
    : processor (p), lookAndFeel (lnf),
      tabs ({ "Kit", "Mixer", "Grooves", "Editor", utf8 ("Música") }),
      kitPage (p), mixerPage (p), groovesPage (p), editorPage (p), songPage (p)
{
    pages = { &kitPage, &mixerPage, &groovesPage, &editorPage, &songPage };

    title.setText ("DRUM WOODYS", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    subtitle.setText ("woody plugins", juce::dontSendNotification);
    subtitle.setFont (juce::FontOptions (12.0f));
    subtitle.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (subtitle);

    status.setFont (juce::FontOptions (13.0f));
    status.setColour (juce::Label::textColourId, colours::accent2);
    status.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (status);

    presetBox.setTooltip (utf8 ("Kits completos: peças, mixer e efeitos"));
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1000 && id - 1000 < (int) userPresets.size())
            processor.applyUserPreset (userPresets[(size_t) (id - 1000)]);
        else if (id >= 1 && id <= (int) factoryPresets().size())
            processor.applyFactoryPreset (factoryPresets()[(size_t) (id - 1)]);
        deletePresetButton.setEnabled (id >= 1000);
    };
    addAndMakeVisible (presetBox);

    savePresetButton.setTooltip (utf8 ("Guarda o kit atual (peças, mixer e efeitos) com um nome"));
    savePresetButton.onClick = [this] { savePreset(); };
    deletePresetButton.onClick = [this] { deletePreset(); };
    addAndMakeVisible (savePresetButton);
    addAndMakeVisible (deletePresetButton);

    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmLabel.setColour (juce::Label::textColourId, colours::textDim);
    bpmLabel.setTooltip (utf8 ("Os grooves seguem o andamento do Ableton"));
    addAndMakeVisible (bpmLabel);

    followButton.setButtonText ("Seguir o Ableton");
    followButton.setTooltip (utf8 ("Quando ligado, o último groove escolhido toca sozinho sempre que você der play no Ableton"));
    followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.parameters, "followHost", followButton);
    addAndMakeVisible (followButton);

    stopButton.onClick = [this] { processor.stop(); };
    addAndMakeVisible (stopButton);

    libraryButton.setTooltip (utf8 ("Escolha a pasta onde ficam os kits (.sfz)"));
    libraryButton.onClick = [this] { chooseLibraryFolder(); };
    addAndMakeVisible (libraryButton);

    tabs.onChange = [this] (int index) { showPage (index); };
    addAndMakeVisible (tabs);

    for (auto* page : pages)
        addChildComponent (*page);

    groovesPage.onEdit = [this] (const Pattern& pattern)
    {
        editorPage.loadPattern (pattern);
        tabs.setSelected (3);
    };
    groovesPage.onAddToSong = [this] (const Pattern& pattern) { addToSong (pattern); };
    editorPage.onAddToSong = [this] (const Pattern& pattern) { addToSong (pattern); };
    editorPage.onSaved = [this]
    {
        groovesPage.reloadUserPatterns();
        showStatus (utf8 ("Salvo em Meus padrões"));
    };

    showPage (0);
    refreshPresets();
    processor.addChangeListener (this);
    startTimerHz (30);
}

MainView::~MainView()
{
    processor.removeChangeListener (this);
}

void MainView::showPage (int index)
{
    for (size_t i = 0; i < pages.size(); ++i)
        pages[i]->setVisible ((int) i == index);
}

void MainView::refreshPresets()
{
    userPresets = UserPresets::load();
    presetBox.clear (juce::dontSendNotification);

    int selectedId = 0;
    presetBox.addSectionHeading (utf8 ("Kits de fábrica"));
    for (size_t i = 0; i < factoryPresets().size(); ++i)
    {
        presetBox.addItem (factoryPresets()[i].name, (int) i + 1);
        if (processor.currentPresetName == factoryPresets()[i].name)
            selectedId = (int) i + 1;
    }

    if (! userPresets.empty())
    {
        presetBox.addSectionHeading ("Meus kits");
        for (size_t i = 0; i < userPresets.size(); ++i)
        {
            const auto name = userPresets[i].getProperty ("name").toString();
            presetBox.addItem (name, 1000 + (int) i);
            if (processor.currentPresetName == name)
                selectedId = 1000 + (int) i;
        }
    }

    presetBox.setTextWhenNothingSelected (utf8 ("Kit personalizado"));
    presetBox.setSelectedId (selectedId, juce::dontSendNotification);
    deletePresetButton.setEnabled (selectedId >= 1000);
}

void MainView::savePreset()
{
    auto* window = new juce::AlertWindow ("Salvar kit", utf8 ("Dê um nome para este kit (peças, mixer e efeitos):"), juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&lookAndFeel);
    window->addTextEditor ("name", processor.currentPresetName.isNotEmpty() ? processor.currentPresetName : juce::String ("Meu kit"));
    window->addButton ("Salvar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, window] (int result)
        {
            const auto name = window->getTextEditorContents ("name").trim();
            if (result != 1 || name.isEmpty())
                return;

            auto presets = UserPresets::load();
            auto preset = processor.capturePreset (name);
            auto existing = std::find_if (presets.begin(), presets.end(), [&] (const juce::ValueTree& t) { return t.getProperty ("name").toString() == name; });
            if (existing != presets.end())
                *existing = preset;
            else
                presets.push_back (preset);

            UserPresets::save (presets);
            processor.currentPresetName = name;
            refreshPresets();
            showStatus ("Kit salvo: " + name);
        }), true);
}

void MainView::deletePreset()
{
    const int index = presetBox.getSelectedId() - 1000;
    if (! juce::isPositiveAndBelow (index, (int) userPresets.size()))
        return;

    const auto name = userPresets[(size_t) index].getProperty ("name").toString();
    userPresets.erase (userPresets.begin() + index);
    UserPresets::save (userPresets);
    processor.currentPresetName = {};
    refreshPresets();
    showStatus (utf8 ("Kit excluído: ") + name);
}

void MainView::addToSong (const Pattern& pattern)
{
    songPage.addPart (pattern);
    showStatus (utf8 ("Adicionado à Música: ") + pattern.name);
}

void MainView::showStatus (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
    statusTicks = 75; // 2.5 s
}

void MainView::chooseLibraryFolder()
{
    chooser = std::make_unique<juce::FileChooser> (utf8 ("Pasta da biblioteca de kits"), processor.getLibrary()->getFolder());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto folder = fc.getResult();
                              if (folder.isDirectory())
                              {
                                  processor.setLibraryFolder (folder);
                                  showStatus (juce::String ((int) processor.getLibrary()->getInstruments().size()) + utf8 (" peças encontradas"));
                              }
                          });
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    kitPage.refresh();
    refreshPresets();
}

void MainView::timerCallback()
{
    for (int s = 0; s < numSlots; ++s)
    {
        const int hits = processor.engine.hitCounters[(size_t) s].load (std::memory_order_relaxed);
        if (hits != lastHits[(size_t) s])
        {
            lastHits[(size_t) s] = hits;
            kitPage.flashSlot (s);
        }
    }

    kitPage.tick();
    if (mixerPage.isVisible())
        mixerPage.updateMeters();

    groovesPage.updatePlayState();
    editorPage.updatePlayState();
    songPage.updatePlayState();
    stopButton.setEnabled (processor.isPlaying());

    // Swing changes apply to whatever is playing
    const float swing = processor.parameters.getRawParameterValue ("swing")->load();
    if (swing != lastSwing)
    {
        lastSwing = swing;
        const auto source = processor.getPlayingSource();
        if (source == GroovesPage::sourceId)     processor.updateSequence (groovesPage.sequence(), source);
        else if (source == EditorPage::sourceId) processor.updateSequence (editorPage.sequence(), source);
        else if (source == SongPage::sourceId)   processor.updateSequence (songPage.sequence(), source);
    }

    bpmLabel.setText (juce::String (processor.getHostBpm(), 1) + " BPM", juce::dontSendNotification);

    if (statusTicks > 0 && --statusTicks == 0)
        status.setText ({}, juce::dontSendNotification);
}

void MainView::paint (juce::Graphics& g)
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
}

void MainView::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    auto header = area.removeFromTop (44);
    header.removeFromLeft (40);
    auto titleArea = header.removeFromLeft (170);
    title.setBounds (titleArea.removeFromTop (26));
    subtitle.setBounds (titleArea);

    header.removeFromLeft (10);
    presetBox.setBounds (header.removeFromLeft (230).withSizeKeepingCentre (230, 32));
    header.removeFromLeft (8);
    savePresetButton.setBounds (header.removeFromLeft (100).withSizeKeepingCentre (100, 32));
    header.removeFromLeft (6);
    deletePresetButton.setBounds (header.removeFromLeft (80).withSizeKeepingCentre (80, 32));

    libraryButton.setBounds (header.removeFromRight (104).withSizeKeepingCentre (104, 32));
    header.removeFromRight (8);
    stopButton.setBounds (header.removeFromRight (104).withSizeKeepingCentre (104, 32));
    header.removeFromRight (12);
    followButton.setBounds (header.removeFromRight (160));
    bpmLabel.setBounds (header.removeFromRight (80));

    area.removeFromTop (12);
    auto tabRow = area.removeFromTop (38);
    tabs.setBounds (tabRow.removeFromLeft (520));
    status.setBounds (tabRow);

    area.removeFromTop (14);
    for (auto* page : pages)
        page->setBounds (area);
}

//==============================================================================
DrumWoodysEditor::DrumWoodysEditor (DrumWoodysProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p, lookAndFeel)
{
    // Read the saved size first: setting the limits below resizes the (still empty) window
    const int width = juce::jlimit (840, 1920, processor.editorWidth > 0 ? processor.editorWidth : MainView::designWidth);

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);
    view.setBounds (0, 0, MainView::designWidth, MainView::designHeight);

    // Free resizing with the design's proportions; the content scales with the window
    setResizable (true, true);
    setResizeLimits (840, 532, 1920, 1216);
    if (auto* sizeLimits = getConstrainer())
        sizeLimits->setFixedAspectRatio ((double) MainView::designWidth / MainView::designHeight);

    setSize (width, width * MainView::designHeight / MainView::designWidth);
    sized = true;
}

DrumWoodysEditor::~DrumWoodysEditor()
{
    setLookAndFeel (nullptr);
}

void DrumWoodysEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sized)
        processor.editorWidth = getWidth();
}
