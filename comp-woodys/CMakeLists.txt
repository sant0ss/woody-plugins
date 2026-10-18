juce_add_plugin(CompWoodys
    COMPANY_NAME "Woody Plugins"
    BUNDLE_ID "com.woodyplugins.compwoodys"
    PLUGIN_MANUFACTURER_CODE Wody
    PLUGIN_CODE CmpW
    FORMATS VST3 Standalone
    PRODUCT_NAME "Comp Woodys"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(CompWoodys PRIVATE
    Source/Compressor.cpp
    Source/Crossover.cpp
    Source/CompEngine.cpp
    Source/CompParams.cpp
    Source/CompWidgets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ../common/WoodyTheme.cpp
    ../common/WoodyWidgets.cpp)

target_include_directories(CompWoodys PRIVATE ../common)

# UI text contains accents
if(MSVC)
    target_compile_options(CompWoodys PRIVATE /utf-8)
endif()

target_compile_definitions(CompWoodys PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CompWoodys
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Numeric checks of the DSP (gain computer, ballistics, lookahead, crossovers); run CompWoodysTests.exe
juce_add_console_app(CompWoodysTests PRODUCT_NAME "CompWoodysTests")
target_sources(CompWoodysTests PRIVATE
    Tests/CompTests.cpp
    Source/Compressor.cpp
    Source/Crossover.cpp
    Source/CompEngine.cpp)
target_include_directories(CompWoodysTests PRIVATE Source ../common)
target_compile_definitions(CompWoodysTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(CompWoodysTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
