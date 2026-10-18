juce_add_plugin(EqWoodys
    COMPANY_NAME "Woody Plugins"
    BUNDLE_ID "com.woodyplugins.eqwoodys"
    PLUGIN_MANUFACTURER_CODE Wody
    PLUGIN_CODE EqWd
    FORMATS VST3 Standalone
    PRODUCT_NAME "EQ Woodys"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EqWoodys PRIVATE
    Source/EqParams.cpp
    Source/FilterDesign.cpp
    Source/LinearPhase.cpp
    Source/EqEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ../common/WoodyTheme.cpp
    ../common/WoodyWidgets.cpp)

target_include_directories(EqWoodys PRIVATE ../common)

# UI text contains accents
if(MSVC)
    target_compile_options(EqWoodys PRIVATE /utf-8)
endif()

target_compile_definitions(EqWoodys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EqWoodys
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Numeric checks of the DSP (filters, linear phase, mid/side, saturation); run EqWoodysTests.exe
juce_add_console_app(EqWoodysTests PRODUCT_NAME "EqWoodysTests")
target_sources(EqWoodysTests PRIVATE
    Tests/EqTests.cpp
    Source/EqParams.cpp
    Source/FilterDesign.cpp
    Source/LinearPhase.cpp
    Source/EqEngine.cpp)
target_include_directories(EqWoodysTests PRIVATE Source ../common)
target_compile_definitions(EqWoodysTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(EqWoodysTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
