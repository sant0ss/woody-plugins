#pragma once

#include "Pages.h"

// Everything inside the plugin window, laid out at a fixed design size and scaled by the editor.
class MainView : public juce::Component,
                 private juce::ChangeListener,
                 private juce::Timer
{
public:
    static constexpr int designWidth = 1200;
    static constexpr int designHeight = 760;

    MainView (DrumWoodysProcessor&, juce::LookAndFeel&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void showPage (int index);
    void refreshPresets();
    void savePreset();
    void deletePreset();
    void chooseLibraryFolder();
    void addToSong (const woody::Pattern&);
    void showStatus (const juce::String&);

    DrumWoodysProcessor& processor;
    juce::LookAndFeel& lookAndFeel;

    juce::Label title, subtitle, status, bpmLabel;
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Salvar kit" };
    juce::TextButton deletePresetButton { "Excluir" };
    juce::TextButton stopButton { "Parar tudo" };
    juce::TextButton libraryButton { "Biblioteca" };
    juce::ToggleButton followButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::ValueTree> userPresets;

    woody::PillTabs tabs;
    woody::KitPage kitPage;
    woody::MixerPage mixerPage;
    woody::GroovesPage groovesPage;
    woody::EditorPage editorPage;
    woody::SongPage songPage;
    std::array<juce::Component*, 5> pages;

    std::array<int, woody::numSlots> lastHits {};
    float lastSwing = -1.0f;
    int statusTicks = 0;
};

class DrumWoodysEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer
{
public:
    explicit DrumWoodysEditor (DrumWoodysProcessor&);
    ~DrumWoodysEditor() override;

    void resized() override;

private:
    DrumWoodysProcessor& processor;
    woody::WoodyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    MainView view;
    bool sized = false; // ignore resizes while the constructor sets things up

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumWoodysEditor)
};
