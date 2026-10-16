#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace woody
{
// One SFZ <region>, merged with its <global>/<master>/<group> opcodes.
// Only describes where the audio is; nothing is loaded here.
struct SfzRegion
{
    juce::File sample;
    int loKey = 0, hiKey = 127;
    int loVel = 1, hiVel = 127;
    float gainDb = 0.0f;
    float tuneCents = 0.0f;
    int seqLength = 1, seqPosition = 1;
    float loRand = 0.0f, hiRand = 1.0f;
};

std::vector<SfzRegion> parseSfz (const juce::File& sfzFile);
} // namespace woody
