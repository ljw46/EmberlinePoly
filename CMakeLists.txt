cmake_minimum_required(VERSION 3.22)

project(EmberlinePoly VERSION 0.1.0 LANGUAGES C CXX)

# Release stage shown next to the version in the UI ("pre-beta", "beta", or "" for a final release).
set(EMBER_VERSION_LABEL "pre-beta" CACHE STRING "Release stage label")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# Audio Unit only exists on macOS
set(EMBERLINE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND EMBERLINE_FORMATS AU)
endif()

juce_add_plugin(EmberlinePoly
    COMPANY_NAME "Emberline"
    PLUGIN_MANUFACTURER_CODE Embl
    PLUGIN_CODE Epol
    PRODUCT_NAME "Emberline Poly"
    FORMATS ${EMBERLINE_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# The audition samples are rendered by tools/make_audition_samples.py and committed; the plugin plays them back with a plain sampler.
file(GLOB EMBERLINE_AUDITION_SAMPLES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/audition/*.wav)
juce_add_binary_data(EmberlineAssets HEADER_NAME AssetData.h NAMESPACE AssetData SOURCES Assets/phoenix.png ${EMBERLINE_AUDITION_SAMPLES})

# The sequencer engine, scale / rate maths, humanization and MIDI export. Depends on juce_audio_basics only, so the
# command-line tests (and CI on Linux) can build it without any GUI libraries.
add_library(EmberlinePolyCore STATIC
    Source/SequencerEngine.cpp
    Source/SequencerEngine.h
    Source/MidiClip.cpp
    Source/MidiClip.h
    Source/Capture.h
    Source/Chords.h
    Source/FactoryPresets.h
    Source/Preset.h
    Source/Randomise.h
    Source/Humanize.h
    Source/Pattern.h
    Source/Rates.h
    Source/Scales.h
    Source/Settings.h
)
set_target_properties(EmberlinePolyCore PROPERTIES POSITION_INDEPENDENT_CODE ON)   # linked into the plugin's shared module
target_include_directories(EmberlinePolyCore PUBLIC Source)
target_link_libraries(EmberlinePolyCore
    PUBLIC
        juce::juce_audio_basics
        juce::juce_core
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
target_compile_definitions(EmberlinePolyCore PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

target_sources(EmberlinePoly PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PatternEditor.cpp
    Source/PatternEditor.h
    Source/ChordStrip.cpp
    Source/ChordStrip.h
    Source/Parameters.cpp
    Source/Parameters.h
    Source/Audition.cpp
    Source/Audition.h
    Source/EmberLookAndFeel.cpp
    Source/EmberLookAndFeel.h
    Source/EditorWidgets.h
    Source/PhoenixLogo.h
    Source/Links.h
    Source/Version.h
)

target_compile_definitions(EmberlinePoly PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    EMBER_VERSION="${PROJECT_VERSION}"
    EMBER_VERSION_LABEL="${EMBER_VERSION_LABEL}"
)

target_link_libraries(EmberlinePoly
    PRIVATE
        EmberlineAssets
        EmberlinePolyCore
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Developer tests: scale / rate maths, the engine's timing and polymeter behaviour, humanization and MIDI export.
#   cmake --build build --target EmberlinePolyTests && build/EmberlinePolyTests_artefacts/Release/EmberlinePolyTests
juce_add_console_app(EmberlinePolyTests PRODUCT_NAME "EmberlinePolyTests")
set_target_properties(EmberlinePolyTests PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(EmberlinePolyTests PRIVATE tests/EngineTests.cpp)
target_link_libraries(EmberlinePolyTests
    PRIVATE
        EmberlinePolyCore
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Developer tool: renders the editor to a PNG and bounces a few bars of generated MIDI to a file.
#   cmake --build build --target EmberlinePolyTool
juce_add_console_app(EmberlinePolyTool PRODUCT_NAME "EmberlinePolyTool")
set_target_properties(EmberlinePolyTool PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(EmberlinePolyTool PRIVATE
    tools/PolyTool.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PatternEditor.cpp
    Source/ChordStrip.cpp
    Source/Parameters.cpp
    Source/Audition.cpp
    Source/EmberLookAndFeel.cpp
)
target_compile_definitions(EmberlinePolyTool PRIVATE
    JucePlugin_Name="Emberline Poly"
    EMBER_VERSION="${PROJECT_VERSION}"
    EMBER_VERSION_LABEL="${EMBER_VERSION_LABEL}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(EmberlinePolyTool
    PRIVATE
        EmberlineAssets
        EmberlinePolyCore
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
