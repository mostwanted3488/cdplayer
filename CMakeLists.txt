cmake_minimum_required(VERSION 3.22)

project(CDPlayerFX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(CDFX_FORMATS VST3)
if(APPLE)
    list(APPEND CDFX_FORMATS AU)
endif()

juce_add_plugin(CDPlayerFX
    COMPANY_NAME "BUYORBEALOSER"
    COMPANY_WEBSITE "https://github.com/mostwanted3488/cdplayer"
    BUNDLE_ID "com.buyorbealoser.cdplayerfx"
    PLUGIN_MANUFACTURER_CODE Bobl
    PLUGIN_CODE Cdpx
    FORMATS ${CDFX_FORMATS}
    PRODUCT_NAME "CD Player FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CDPlayerFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Display.cpp
    Source/Dialogs.cpp
    Source/LookAndFeel95.cpp)

target_compile_definitions(CDPlayerFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CDPlayerFX
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless engine test (cmake -DCDFX_TESTS=ON)
option(CDFX_TESTS "Build the headless engine test" OFF)
if(CDFX_TESTS)
    juce_add_console_app(CDPlayerFXTest PRODUCT_NAME "CDPlayerFXTest")
    target_sources(CDPlayerFXTest PRIVATE
        tests/EngineTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Display.cpp
        Source/Dialogs.cpp
        Source/LookAndFeel95.cpp)
    target_compile_definitions(CDPlayerFXTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="CD Player FX")
    target_link_libraries(CDPlayerFXTest PRIVATE juce::juce_audio_processors juce::juce_gui_extra juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
