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
    std::function<juce::String (int)> detail; // right-aligned, dimmed
    std::function<void (int)> onSelect;
    std::function<void (int)> onDoubleClick;

    int getNumRows() override { return numRows != nullptr ? numRows() : 0; }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
};

// Section caption used inside panels.
class SectionLabel : public juce::Label
{
public:
    explicit SectionLabel (const juce::String& text);
};

//==============================================================================
// The kit drawn as abstract shapes: rings for cymbals, discs for drums, diamonds for percussion.
class KitMap : public juce::Component
{
public:
    explicit KitMap (DrumWoodysProcessor&);

    std::function<void (int slot)> onSelect;
    void setSelected (int slot);
    void flash (int slot);
    void tick();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Shape
    {
        float x, y, radius; // in a virtual 800 x 460 canvas
    };

    static const std::array<Shape, numSlots>& shapes();
    juce::AffineTransform canvasTransform() const;
    juce::Rectangle<float> shapeBounds (int slot) const;
    int slotAt (juce::Point<float>) const;

    DrumWoodysProcessor& processor;
    std::array<float, numSlots> flashLevel {};
    int selected = kick;
    int heldNote = -1;
};

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
    juce::Colour colour;
    juce::String label;
    float flashLevel = 0.0f;
};

// Everything about one kit piece: which sample, articulations, mix, EQ, dynamics, output.
class SlotPanel : public juce::Component
{
public:
    SlotPanel (DrumWoodysProcessor&, int slot);

    void refresh();
    void decay();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    DrumWoodysProcessor& processor;
    int slot;
    juce::Colour colour;

    juce::Label title;
    juce::TextButton mute { "M" }, solo { "S" };
    std::unique_ptr<ButtonAttachment> muteAttachment, soloAttachment;
    juce::ComboBox piece;
    std::vector<juce::String> itemIds; // ComboBox item id - 1 -> instrument id
    std::vector<std::unique_ptr<ArticulationPad>> pads;

    SectionLabel mixLabel { "MIX" }, eqLabel { "EQ" }, dynamicsLabel { utf8 ("DINÂMICA") };
    LabeledKnob level, pan, tune, overhead, room;
    LabeledKnob eqLow, eqMid, eqFreq, eqHigh;
    LabeledKnob comp, attack, sustain, drive;
    juce::ToggleButton separateOut;
    std::unique_ptr<ButtonAttachment> outAttachment;
};

class KitPage : public juce::Component
{
public:
    explicit KitPage (DrumWoodysProcessor&);

    void refresh();
    void flashSlot (int slot);
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void select (int slot);

    DrumWoodysProcessor& processor;
    KitMap map;
    juce::Label hint;
    std::array<std::unique_ptr<SlotPanel>, numSlots> panels;
    int selected = kick;
    juce::Rectangle<int> mapArea;
};

//==============================================================================
class MixerPage : public juce::Component
{
public:
    explicit MixerPage (DrumWoodysProcessor&);

    void updateMeters();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Strip : public juce::Component
    {
        Strip (DrumWoodysProcessor&, const juce::String& name, juce::Colour,
               const juce::String& levelId, const juce::String& muteId, const juce::String& soloId, const juce::String& panId);
        void paint (juce::Graphics&) override;
        void resized() override;

        juce::Colour colour;
        juce::Label name;
        LevelMeter meter;
        juce::Slider level { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
        juce::TextButton mute { "M" }, solo { "S" };
        std::unique_ptr<LabeledKnob> pan;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment, soloAttachment;
    };

    DrumWoodysProcessor& processor;
    std::vector<std::unique_ptr<Strip>> strips; // 14 pieces, overhead, room

    juce::Label masterLabel;
    LevelMeter masterMeter;
    juce::Slider master { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::AudioProcessorValueTreeState::SliderAttachment masterAttachment;
    LabeledKnob glue, roomSize, roomComp, humanize, variation;

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
    void refreshPatternList();
    void selectPattern (int index);
    void showPattern (const Pattern&);
    void togglePlay();
    bool isUserGenre() const;

    DrumWoodysProcessor& processor;
    std::vector<Pattern> userPatterns;
    juce::StringArray genres;
    std::vector<Pattern> patterns; // of the selected genre, after the filter
    Pattern base;                  // chosen in the list
    Pattern selected;              // base or a generated variation of it
    bool hasSelection = false;
    int seed = 1;

    SimpleListModel genreModel, patternModel;
    juce::ListBox genreList { "genres" }, patternList { "patterns" };
    juce::ComboBox complexityFilter;

    juce::Label title, info;
    juce::TextButton playButton { "Tocar" };
    juce::TextButton varyButton { "Variar" };
    juce::TextButton fillButton { "Virada" };
    juce::TextButton editButton { "Editar" };
    juce::TextButton addButton { utf8 ("Adicionar à Música") };
    juce::TextButton deleteButton { "Excluir" };
    LabeledKnob swing, timing;
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
    bool keyPressed (const juce::KeyPress&) override;

private:
    void refreshControls();
    void patternChanged();
    void togglePlay();
    void saveToUserPatterns();
    void pushUndo();
    void undo();
    void redo();
    void replacePattern (const Pattern&);

    DrumWoodysProcessor& processor;
    std::vector<Pattern> undoStack, redoStack;
    int seed = 1;

    juce::Label nameLabel, barsLabel, gridLabel, help;
    juce::TextEditor nameEditor;
    juce::ComboBox barsBox, gridBox;
    juce::TextButton playButton { "Tocar" };
    juce::TextButton undoButton { "Desfazer" };
    juce::TextButton redoButton { "Refazer" };
    juce::TextButton varyButton { "Variar" };
    juce::TextButton fillButton { "Virada" };
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
        MidiDragButton drag { "MIDI", true };
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
