cmake_minimum_required(VERSION 3.22)

# Enoch Bass is a Windows-only VST3 instrument.
if(NOT WIN32)
    message(FATAL_ERROR "Enoch Bass is Windows-only. Build it on Windows (see README.md).")
endif()

# static MSVC runtime: the plugin runs on any Windows PC without installing redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(EnochBass VERSION 1.5.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(EnochBass
    PRODUCT_NAME "Enoch Bass"
    COMPANY_NAME "Enoch"
    PLUGIN_MANUFACTURER_CODE Enoc
    PLUGIN_CODE EnBs
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EnochBass PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/RollView.cpp
    Source/SoundFontImpl.cpp)

target_compile_definitions(EnochBass PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(EnochBass PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_events
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(EnochBass PRIVATE /utf-8 /bigobj)
endif()
