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

    # NB: JUCE has no native CLAP (unknown tokens here are silently ignored);
    # CLAP is added via clap-juce-extensions below.
    FORMATS                     Standalone AU VST3

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

# Preset backend (CONTROL panel): the manager plus setBfree's .pgm parser
# (parse-only — installProgram's engine callbacks are never invoked).
set(PRESET_SOURCES
    plugin/PresetManager.cpp
    src/pgmParser.cpp
    src/program.cpp
)

target_sources(tuneBfree PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/midi_stubs.cpp
    ${DSP_SOURCES}
    ${PRESET_SOURCES}
)

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(tuneBfree PRIVATE
    src
    libs/MTS-ESP/Client
    libs/readerwriterqueue
    libs/tuning-library/include
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
# Melatonin Inspector (GUI layout debugging). Auto-enabled when the submodule
# is present. To install it (needs network, so run this yourself):
#   git submodule add https://github.com/sudara/melatonin_inspector.git libs/melatonin_inspector
#   git submodule update --init libs/melatonin_inspector
# Then reconfigure cmake. Toggle the inspector with cmd+I in the editor.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/melatonin_inspector/CMakeLists.txt")
    add_subdirectory(libs/melatonin_inspector)
    target_link_libraries(tuneBfree PRIVATE melatonin_inspector)
    target_compile_definitions(tuneBfree PRIVATE TUNEBFREE_MELATONIN=1)
    message(STATUS "Melatonin Inspector enabled (cmd+I in the editor)")
endif()

# ---------------------------------------------------------------------------
# CLAP via clap-juce-extensions (JUCE itself has no CLAP support).
# Auto-enabled when the submodule is present. To install it (needs network,
# so run this yourself — note the --recursive, it has its own submodules):
#   git submodule add https://github.com/free-audio/clap-juce-extensions.git libs/clap-juce-extensions
#   git submodule update --init --recursive libs/clap-juce-extensions
# Then reconfigure cmake. Build target: tuneBfree_CLAP; artifact lands in
# build/tuneBfree_artefacts/<config>/CLAP/tuneBfree.clap.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/clap-juce-extensions/CMakeLists.txt")
    add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)
    clap_juce_extensions_plugin(TARGET tuneBfree
        CLAP_ID "com.tunebfree.plugin"
        CLAP_FEATURES instrument organ)
    message(STATUS "CLAP enabled via clap-juce-extensions (target: tuneBfree_CLAP)")
endif()

# ---------------------------------------------------------------------------
# Unit tests (doctest) — JUCE-free console runner.
# Builds the DSP core + config stubs with -DTESTS, so the in-file TEST_CASE
# blocks (src/tuning.cpp, src/tonegen.cpp) compile into one executable.
#
#   git submodule update --init libs/doctest
#   cmake -B build -DTUNEBFREE_BUILD_TESTS=ON
#   cmake --build build --target tuneBfree_tests
#   ctest --test-dir build --output-on-failure   # or run the exe directly
#
# In VSCode (CMake Tools): set TUNEBFREE_BUILD_TESTS in settings, then use the
# "Run CTest" button in the status bar.
# ---------------------------------------------------------------------------
option(TUNEBFREE_BUILD_TESTS "Build the doctest unit tests" OFF)
if(TUNEBFREE_BUILD_TESTS)
    enable_testing()
    add_executable(tuneBfree_tests
        ${DSP_SOURCES}
        plugin/midi_stubs.cpp
    )
    target_compile_definitions(tuneBfree_tests PRIVATE
        TESTS
        TUNEBFREE_TUNINGS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tunings"
    )
    target_include_directories(tuneBfree_tests PRIVATE
        src
        libs/MTS-ESP/Client
        libs/tuning-library/include
        libs/doctest/doctest
    )
    add_test(NAME tuneBfree_tests COMMAND tuneBfree_tests)
endif()

# ---------------------------------------------------------------------------
# Headless GUI snapshot tool (dev). Renders the editor to a PNG in software —
# no window, no audio/MIDI device, no screen access — for the screenshot-review
# workflow (safe to run sandboxed).
#
#   cmake --build build --target tuneBfree_snapshot
#   build/tuneBfree_snapshot_artefacts/Release/tuneBfree_snapshot 1 tinker.png
# ---------------------------------------------------------------------------
option(TUNEBFREE_BUILD_SNAPSHOT_TOOL "Build the headless GUI snapshot tool" ON)
if(TUNEBFREE_BUILD_SNAPSHOT_TOOL)
    juce_add_console_app(tuneBfree_snapshot PRODUCT_NAME "tuneBfree_snapshot")
    juce_generate_juce_header(tuneBfree_snapshot)
    target_sources(tuneBfree_snapshot PRIVATE
        scripts/SnapshotTool.cpp
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/midi_stubs.cpp
        ${DSP_SOURCES}
        ${PRESET_SOURCES}
    )
    target_include_directories(tuneBfree_snapshot PRIVATE
        src
        plugin
        libs/MTS-ESP/Client
        libs/readerwriterqueue
        libs/tuning-library/include
    )
    target_compile_definitions(tuneBfree_snapshot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=0
    )
    target_link_libraries(tuneBfree_snapshot PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
endif()

# ---------------------------------------------------------------------------
# Headless offline audio render tool (dev). Drives the DSP with no audio/MIDI
# device and writes a WAV — the audio counterpart of the snapshot tool, and the
# sandbox-safe way to check a render for NaN/Inf, silence, level, and tuning.
#
#   cmake --build build --target tuneBfree_render
#   build/tuneBfree_render_artefacts/Release/tuneBfree_render out.wav --note 84 \
#       --drawbars 0,0,0,0,0,0,0,0,8 --scl tunings/9ed3halves/9ed3halves.scl
# ---------------------------------------------------------------------------
option(TUNEBFREE_BUILD_RENDER_TOOL "Build the headless audio render tool" ON)
if(TUNEBFREE_BUILD_RENDER_TOOL)
    juce_add_console_app(tuneBfree_render PRODUCT_NAME "tuneBfree_render")
    juce_generate_juce_header(tuneBfree_render)
    target_sources(tuneBfree_render PRIVATE
        scripts/AudioRenderTool.cpp
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/midi_stubs.cpp
        ${DSP_SOURCES}
        ${PRESET_SOURCES}
    )
    target_include_directories(tuneBfree_render PRIVATE
        src
        plugin
        libs/MTS-ESP/Client
        libs/readerwriterqueue
        libs/tuning-library/include
    )
    target_compile_definitions(tuneBfree_render PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=0
    )
    target_link_libraries(tuneBfree_render PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    )
endif()

# ---------------------------------------------------------------------------
# Legacy CLAP build (original implementation, kept for reference)
# Build with: cmake -S src -B src/build
# ---------------------------------------------------------------------------
