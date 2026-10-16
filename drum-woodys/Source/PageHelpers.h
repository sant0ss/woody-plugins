#pragma once

#include "PluginProcessor.h"
#include "WoodyLookAndFeel.h"

namespace woody::helpers
{
// Plays a note once from the UI.
inline void audition (DrumWoodysProcessor& processor, int note, float velocity)
{
    processor.keyboardState.noteOn (1, note, velocity);
    processor.keyboardState.noteOff (1, note, 0.0f);
}

// Ableton names middle C (60) as C3
inline juce::String noteName (int note)
{
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
}

inline float swingValue (DrumWoodysProcessor& processor)
{
    return processor.parameters.getRawParameterValue ("swing")->load();
}

inline int playStepFor (DrumWoodysProcessor& processor, const char* source, const Pattern& pattern)
{
    if (! processor.isPlaying() || processor.getPlayingSource() != source)
        return -1;

    const double position = processor.getPlayPosition();
    return position < 0.0 ? -1 : (int) (position * pattern.stepsPerBar / 4.0);
}

inline void styleTitle (juce::Label& label, float size)
{
    label.setFont (juce::FontOptions (size, juce::Font::bold));
    label.setColour (juce::Label::textColourId, colours::text);
}

inline void styleHint (juce::Label& label)
{
    label.setFont (juce::FontOptions (13.0f));
    label.setColour (juce::Label::textColourId, colours::textDim);
}

inline void fillPanel (juce::Graphics& g, juce::Rectangle<int> area, float radius = 14.0f)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (area.toFloat(), radius);
}
} // namespace woody::helpers
