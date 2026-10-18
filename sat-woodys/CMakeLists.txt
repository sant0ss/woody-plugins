juce_add_plugin(SatWoodys
    COMPANY_NAME "Woody Plugins"
    BUNDLE_ID "com.woodyplugins.satwoodys"
    PLUGIN_MANUFACTURER_CODE Wody
    PLUGIN_CODE SatW
    FORMATS VST3 Standalone
    PRODUCT_NAME "Sat Woodys"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SatWoodys PRIVATE
    Source/SatParams.cpp
    Source/SatEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ../common/WoodyTheme.cpp
    ../common/WoodyWidgets.cpp)

target_include_directories(SatWoodys PRIVATE ../common)

# UI text contains accents
if(MSVC)
    target_compile_options(SatWoodys PRIVATE /utf-8)
endif()

target_compile_definitions(SatWoodys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SatWoodys
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Numeric checks of the DSP (curves, mix, latency, stereo modes, focus, oversampling); run SatWoodysTests.exe
juce_add_console_app(SatWoodysTests PRODUCT_NAME "SatWoodysTests")
target_sources(SatWoodysTests PRIVATE
    Tests/SatTests.cpp
    Source/SatParams.cpp
    Source/SatEngine.cpp
    ../common/WoodyTheme.cpp)
target_include_directories(SatWoodysTests PRIVATE Source ../common)
if(MSVC)
    target_compile_options(SatWoodysTests PRIVATE /utf-8)
endif()
target_compile_definitions(SatWoodysTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SatWoodysTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
