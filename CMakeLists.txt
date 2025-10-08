cmake_minimum_required(VERSION 3.15)

project(ProTune VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

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
    FORMATS VST3 Standalone
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
    PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

if (MSVC)
    target_compile_options(ProTune PRIVATE /W4 /permissive- /bigobj)
else()
    target_compile_options(ProTune PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(ProTune PRIVATE juce::juce_audio_utils juce::juce_dsp)

add_executable(VST3Probe
    Tools/VST3Probe.cpp
)

target_compile_definitions(VST3Probe PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_STANDALONE_APPLICATION=1 JUCE_VST3_LOGGING=1)

target_link_libraries(VST3Probe PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
)
