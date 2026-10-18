#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CompEngine.h"

namespace compwoody
{
juce::String bandId (int band, const char* name); // "b1_thr" ...
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Same ratio range for the single band and the multiband bands: 1:1 to 20:1, then the limit setting
juce::NormalisableRange<float> ratioRange();
juce::String ratioText (float ratio);

juce::StringArray lookaheadNames();

// Raw parameter pointers, resolved once; read() is safe on the audio thread
class ParamSet
{
public:
    explicit ParamSet (juce::AudioProcessorValueTreeState&);

    EngineParams read() const;

private:
    struct Band
    {
        std::atomic<float> *threshold, *ratio, *attack, *release, *makeup, *solo, *bypass;
    };

    std::atomic<float> *mode, *bypass, *threshold, *ratio, *attack, *release, *autoRelease, *knee, *makeup, *autoMakeup,
        *mix, *scHighPass, *link, *detector, *lookahead, *channel;
    std::array<std::atomic<float>*, numCrossovers> crossovers;
    std::array<Band, numBands> bands;
};
} // namespace compwoody
