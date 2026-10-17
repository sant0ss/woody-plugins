juce_add_plugin(SynthWoodys
    COMPANY_NAME "Woody Plugins"
    BUNDLE_ID "com.woodyplugins.synthwoodys"
    PLUGIN_MANUFACTURER_CODE Wody
    PLUGIN_CODE SyWd
    FORMATS VST3 Standalone
    PRODUCT_NAME "Synth Woodys"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SynthWoodys PRIVATE
    Source/Params.cpp
    Source/Wavetables.cpp
    Source/SynthEngine.cpp
    Source/Effects.cpp
    Source/PluginProcessor.cpp
    Source/Components.cpp
    Source/PluginEditor.cpp
    ../common/WoodyTheme.cpp)

target_include_directories(SynthWoodys PRIVATE ../common)

# UI text contains accents
if(MSVC)
    target_compile_options(SynthWoodys PRIVATE /utf-8)
endif()

target_compile_definitions(SynthWoodys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SynthWoodys
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
