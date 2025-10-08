cmake_minimum_required(VERSION 3.15)

project(ProTune VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow the user to provide a JUCE path via -DJUCE_DIR or JUCE_SOURCE_DIR.
if (NOT DEFINED JUCE_DIR AND NOT DEFINED JUCE_SOURCE_DIR)
    message(FATAL_ERROR "Please configure JUCE by setting JUCE_DIR (for CMake package) or JUCE_SOURCE_DIR (for add_subdirectory).")
endif()

if (DEFINED JUCE_DIR)
    find_package(JUCE CONFIG REQUIRED)
elseif (DEFINED JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce)
endif()

juce_add_plugin(ProTune
    COMPANY_NAME "OpenAI"
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE Prot
    FORMATS VST3
    PRODUCT_NAME "ProTune"
    BUNDLE_ID com.openai.protune
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(ProTune)

target_sources(ProTune PRIVATE
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/PitchCorrectionEngine.cpp
)

target_compile_definitions(ProTune
    PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

if (MSVC)
    target_compile_options(ProTune PRIVATE /W4 /permissive- /bigobj)
else()
    target_compile_options(ProTune PRIVATE -Wall -Wextra -Wpedantic)
endif()

juce_add_binary_data(ProTuneData
    SOURCES
)

target_link_libraries(ProTune PRIVATE juce::juce_audio_utils ProTuneData)
