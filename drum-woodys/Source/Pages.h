#pragma once

#include "PluginProcessor.h"
#include "UiComponents.h"

namespace woody
{
// A ListBoxModel driven by lambdas.
struct SimpleListModel : public juce::ListBoxModel
{
    std::function<int()> numRows;
    std::function<juce::String (int)> text;
    std::function<void (int)> onSelect;
    std::function<void (int)> onDoubleClick;

    int getNumRows() override { return numRows != nullptr ? numRows() : 0; }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
};

//==============================================================================
// Plays one articulation of a slot while the mouse is held. Higher = louder.
class ArticulationPad : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    ArticulationPad (DrumWoodysProcessor&, int slot, int articulation);

    void flash() { flashLevel = 1.0f; repaint(); }
    void decay();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    DrumWoodysProcessor& processor;
    int note;
    juce::String label;
    float flashLevel = 0.0f;
};

// One kit position: piece selector, articulation pads and tuning.
class SlotCard : public juce::Component
{
public:
    SlotCard (DrumWoodysProcessor&, int slot);

    void refresh();
    void flash();
    void decay();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DrumWoodysProcessor& processor;
    int slot;
    juce::Label title;
    juce::ComboBox piece;
    std::vector<juce::String> itemIds; // ComboBox item id - 1 -> instrument id
    std::vector<std::unique_ptr<ArticulationPad>> pads;
    LabeledKnob tune;
    float flashLevel = 0.0f;
};

class KitPage : public juce::Component
{
public:
    explicit KitPage (DrumWoodysProcessor&);

    void refresh();
    void flashSlot (int slot) { cards[(size_t) slot]->flash(); }
    void decay();

    void resized() override;

private:
    DrumWoodysProcessor& processor;
    juce::Label hint;
    juce::TextButton resetButton { utf8 ("Kit padrão") };
    std::array<std::unique_ptr<SlotCard>, numSlots> cards;
};

//==============================================================================
class MixerPage : public juce::Component
{
public:
    explicit MixerPage (DrumWoodysProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Strip : public juce::Component
    {
        Strip (DrumWoodysProcessor&, int slot);
        void resized() override;

        juce::Label name;
        juce::TextButton mute { "M" }, solo { "S" };
        juce::Slider level { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
        LabeledKnob pan, room;
        juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;
        juce::AudioProcessorValueTreeState::ButtonAttachment muteAttachment, soloAttachment;
    };

    std::array<std::unique_ptr<Strip>, numSlots> strips;

    juce::Label masterLabel;
    juce::Slider master { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::AudioProcessorValueTreeState::SliderAttachment masterAttachment;
    LabeledKnob ambience, roomSize, humanize;

    juce::Rectangle<int> stripsArea, masterArea;
};

//==============================================================================
class GroovesPage : public juce::Component
{
public:
    explicit GroovesPage (DrumWoodysProcessor&);

    static constexpr auto sourceId = "groove";

    Sequence sequence() const;
    void reloadUserPatterns();
    void updatePlayState();

    std::function<void (const Pattern&)> onEdit;
    std::function<void (const Pattern&)> onAddToSong;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void selectGenre (int index);
    void selectPattern (int index);
    void togglePlay();
    bool isUserGenre() const;

    DrumWoodysProcessor& processor;
    std::vector<Pattern> userPatterns;
    juce::StringArray genres;
    std::vector<Pattern> patterns; // of the selected genre
    Pattern selected;
    bool hasSelection = false;

    SimpleListModel genreModel, patternModel;
    juce::ListBox genreList { "genres" }, patternList { "patterns" };

    juce::Label title, info;
    juce::TextButton playButton { "Tocar" };
    juce::TextButton editButton { "Editar no Editor" };
    juce::TextButton addButton { utf8 ("Adicionar à Música") };
    juce::TextButton deleteButton { "Excluir" };
    LabeledKnob swing;
    MidiDragButton drag { "Arraste para o Ableton (MIDI)" };
    StepGrid preview;

    juce::Rectangle<int> detailArea;
};

//==============================================================================
class EditorPage : public juce::Component
{
public:
    explicit EditorPage (DrumWoodysProcessor&);

    static constexpr auto sourceId = "edit";

    void loadPattern (const Pattern&);
    Sequence sequence() const;
    void updatePlayState();

    std::function<void()> onSaved;
    std::function<void (const Pattern&)> onAddToSong;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshControls();
    void patternChanged();
    void togglePlay();
    void saveToUserPatterns();

    DrumWoodysProcessor& processor;

    juce::Label nameLabel, barsLabel, gridLabel, help;
    juce::TextEditor nameEditor;
    juce::ComboBox barsBox, gridBox;
    juce::TextButton playButton { "Tocar" };
    juce::TextButton clearButton { "Limpar" };
    juce::TextButton saveButton { utf8 ("Salvar em Meus padrões") };
    juce::TextButton addButton { utf8 ("Adicionar à Música") };
    MidiDragButton drag { "Arraste para o Ableton (MIDI)" };
    juce::Viewport viewport;
    StepGrid grid;
};

//==============================================================================
class SongPage : public juce::Component
{
public:
    explicit SongPage (DrumWoodysProcessor&);

    static constexpr auto sourceId = "song";

    void addPart (const Pattern&);
    Sequence sequence() const;
    void updatePlayState();
    void rebuild();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class PartBlock : public juce::Component
    {
    public:
        PartBlock (SongPage&, int index);
        void setHighlighted (bool);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        SongPage& page;
        int index;
        bool highlighted = false;
        juce::Label name, genre, repeats;
        juce::TextButton minus { "-" }, plus { "+" }, left { "<" }, right { ">" }, remove { "x" };
    };

    void togglePlay();
    void changed();

    DrumWoodysProcessor& processor;
    juce::TextButton playButton { utf8 ("Tocar música") };
    juce::TextButton clearButton { "Limpar" };
    MidiDragButton drag { utf8 ("Arraste a música inteira para o Ableton (MIDI)") };
    juce::Label total, empty;
    juce::Viewport viewport;
    juce::Component strip;
    std::vector<std::unique_ptr<PartBlock>> blocks;
};
} // namespace woody
