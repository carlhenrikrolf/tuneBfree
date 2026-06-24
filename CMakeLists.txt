cmake_minimum_required(VERSION 3.22)
project(tuneBfree VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# JUCE
# Preferred: git submodule at libs/JUCE
#   git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE
#   git submodule update --init --recursive
# Fallback: downloads at configure time via FetchContent
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    add_subdirectory(libs/JUCE)
else()
    message(STATUS "libs/JUCE not found — downloading via FetchContent (requires internet)")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.7
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(tuneBfree
    VERSION                     2.0.0
    COMPANY_NAME                "tuneBfree"
    BUNDLE_ID                   "com.tunebfree.plugin"
    DESCRIPTION                 "Tonewheel organ with microtuning"

    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE

    # These four chars must be unique across plugins in a DAW.
    # Uppercase first char required by Apple for AU.
    PLUGIN_MANUFACTURER_CODE    "Tunb"
    PLUGIN_CODE                 "Tfre"

    FORMATS                     Standalone AU VST3 CLAP

    PRODUCT_NAME                "tuneBfree"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice

    BLUETOOTH_PERMISSION_ENABLED    TRUE
    BLUETOOTH_PERMISSION_TEXT       "tuneBfree uses Bluetooth to receive MIDI from wireless controllers."
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "tuneBfree uses the microphone for audio input through the Leslie speaker."
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
juce_generate_juce_header(tuneBfree)

set(DSP_SOURCES
    src/tonegen.cpp
    src/tuning.cpp
    src/vibrato.cpp
    src/whirl.cpp
    src/eqcomp.cpp
    src/overdrive.cpp
    src/reverb.cpp
    libs/MTS-ESP/Client/libMTSClient.cpp
)

target_sources(tuneBfree PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/midi_stubs.cpp
    ${DSP_SOURCES}
)

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(tuneBfree PRIVATE
    src
    libs/MTS-ESP/Client
    libs/readerwriterqueue
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(tuneBfree PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ---------------------------------------------------------------------------
# Link libraries
# juce_recommended_lto_flags can cause issues on RPi; remove if build hangs.
# ---------------------------------------------------------------------------
target_link_libraries(tuneBfree
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Legacy CLAP build (original implementation, kept for reference)
# Build with: cmake -S src -B src/build
# ---------------------------------------------------------------------------
