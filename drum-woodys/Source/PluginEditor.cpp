#include "PluginEditor.h"

using namespace woody;

DrumWoodysEditor::DrumWoodysEditor (DrumWoodysProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      kitPage (p), mixerPage (p), groovesPage (p), editorPage (p), songPage (p)
{
    setLookAndFeel (&lookAndFeel);

    title.setText ("DRUM WOODYS", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (title);

    status.setFont (juce::FontOptions (13.0f));
    status.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (status);

    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmLabel.setColour (juce::Label::textColourId, colours::textDim);
    bpmLabel.setTooltip (utf8 ("Os grooves seguem o andamento do Ableton"));
    addAndMakeVisible (bpmLabel);

    stopButton.onClick = [this] { processor.stop(); };
    addAndMakeVisible (stopButton);

    followButton.setButtonText ("Tocar junto com o Ableton");
    followButton.setColour (juce::ToggleButton::textColourId, colours::text);
    followButton.setColour (juce::ToggleButton::tickColourId, colours::accent);
    followButton.setColour (juce::ToggleButton::tickDisabledColourId, colours::textDim);
    followButton.setTooltip (utf8 ("Quando ligado, o último groove escolhido toca sozinho sempre que você der play no Ableton"));
    followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.parameters, "followHost", followButton);
    addAndMakeVisible (followButton);

    libraryButton.setTooltip (utf8 ("Escolha a pasta onde ficam os kits (.sfz)"));
    libraryButton.onClick = [this] { chooseLibraryFolder(); };
    addAndMakeVisible (libraryButton);

    groovesPage.onEdit = [this] (const Pattern& pattern)
    {
        editorPage.loadPattern (pattern);
        tabs.setCurrentTabIndex (3);
    };
    groovesPage.onAddToSong = [this] (const Pattern& pattern) { addToSong (pattern); };
    editorPage.onAddToSong = [this] (const Pattern& pattern) { addToSong (pattern); };
    editorPage.onSaved = [this]
    {
        groovesPage.reloadUserPatterns();
        showStatus (utf8 ("Salvo em Meus padrões"));
    };

    tabs.setTabBarDepth (34);
    tabs.setOutline (0);
    tabs.addTab ("Kit", colours::panel, &kitPage, false);
    tabs.addTab ("Mixer", colours::panel, &mixerPage, false);
    tabs.addTab ("Grooves", colours::panel, &groovesPage, false);
    tabs.addTab ("Editor", colours::panel, &editorPage, false);
    tabs.addTab (utf8 ("Música"), colours::panel, &songPage, false);
    addAndMakeVisible (tabs);

    processor.addChangeListener (this);

    setSize (1200, 720);
    startTimerHz (30);
}

DrumWoodysEditor::~DrumWoodysEditor()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void DrumWoodysEditor::addToSong (const Pattern& pattern)
{
    songPage.addPart (pattern);
    showStatus (utf8 ("Adicionado à Música: ") + pattern.name);
}

void DrumWoodysEditor::showStatus (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
    statusTicks = 75; // 2.5 s
}

void DrumWoodysEditor::chooseLibraryFolder()
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

void DrumWoodysEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    kitPage.refresh();
}

void DrumWoodysEditor::timerCallback()
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
    kitPage.decay();

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

void DrumWoodysEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void DrumWoodysEditor::resized()
{
    auto area = getLocalBounds().reduced (12, 8);

    auto header = area.removeFromTop (40);
    title.setBounds (header.removeFromLeft (200));
    libraryButton.setBounds (header.removeFromRight (120).reduced (0, 5));
    header.removeFromRight (10);
    stopButton.setBounds (header.removeFromRight (100).reduced (0, 5));
    header.removeFromRight (10);
    followButton.setBounds (header.removeFromRight (220));
    bpmLabel.setBounds (header.removeFromRight (90));
    status.setBounds (header);

    area.removeFromTop (4);
    tabs.setBounds (area);
}
