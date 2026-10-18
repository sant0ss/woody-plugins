#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace satwoody
{
enum StereoMode { Stereo, MidOnly, SideOnly };

constexpr float maxDriveDb = 36.0f;
constexpr float focusLowMin = 20.0f, focusLowMax = 2000.0f;
constexpr float focusHighMin = 500.0f, focusHighMax = 20000.0f;

juce::StringArray typeNames();
juce::StringArray stereoNames();
juce::StringArray oversamplingNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace satwoody
