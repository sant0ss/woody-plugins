juce_add_plugin(DrumWoodys
    COMPANY_NAME "Woody Plugins"
    BUNDLE_ID "com.woodyplugins.drumwoodys"
    PLUGIN_MANUFACTURER_CODE Wody
    PLUGIN_CODE DrWd
    FORMATS VST3 Standalone
    PRODUCT_NAME "Drum Woodys"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(DrumWoodys PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Pages.cpp
    Source/UiComponents.cpp
    Source/SfzParser.cpp
    Source/Library.cpp
    Source/DrumEngine.cpp
    Source/Patterns.cpp
    Source/PatternLibrary.cpp
    Source/WoodyLookAndFeel.cpp)

# UI text and the groove library contain accents
if(MSVC)
    target_compile_options(DrumWoodys PRIVATE /utf-8)
endif()

target_compile_definitions(DrumWoodys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DrumWoodys
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
