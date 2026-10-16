#pragma once

#include "Pages.h"

class DrumWoodysEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    explicit DrumWoodysEditor (DrumWoodysProcessor&);
    ~DrumWoodysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void chooseLibraryFolder();
    void addToSong (const woody::Pattern&);
    void showStatus (const juce::String&);

    DrumWoodysProcessor& processor;
    woody::WoodyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Label title, status, bpmLabel;
    juce::TextButton stopButton { "Parar tudo" };
    juce::TextButton libraryButton { "Biblioteca..." };
    juce::ToggleButton followButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    woody::KitPage kitPage;
    woody::MixerPage mixerPage;
    woody::GroovesPage groovesPage;
    woody::EditorPage editorPage;
    woody::SongPage songPage;

    std::array<int, woody::numSlots> lastHits {};
    float lastSwing = -1.0f;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumWoodysEditor)
};
