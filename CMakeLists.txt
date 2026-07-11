cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  CHRONA — modern temporal-manipulation effect (VST3 / AU / Standalone)
#
#  Conceptually inspired by time-warp effects, but an original design and
#  implementation. Strictly modular: DSP / automation / presets / UI /
#  visualizer / MIDI / rendering are separated into independent units.
# ============================================================================

project(CHRONA VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# ---------------------------------------------------------------------------
#  JUCE: fetched at configure time so the tree stays clean. Pin a tag for
#  reproducible builds. Override with -DCHRONA_JUCE_TAG=... if needed.
# ---------------------------------------------------------------------------
option(CHRONA_BUILD_STANDALONE "Build the Standalone target" ON)
# Installing into the system plug-in folders after each build is convenient
# locally but breaks sandboxed CI / validator machines — off by default there.
option(CHRONA_COPY_AFTER_BUILD "Copy the plugin into the user plug-in folders after build" ON)
set(CHRONA_JUCE_TAG "8.0.4" CACHE STRING "JUCE git tag to fetch")

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${CHRONA_JUCE_TAG}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# ---------------------------------------------------------------------------
#  Plugin formats
# ---------------------------------------------------------------------------
set(CHRONA_FORMATS VST3 AU)
if(CHRONA_BUILD_STANDALONE)
    list(APPEND CHRONA_FORMATS Standalone)
endif()

juce_add_plugin(CHRONA
    COMPANY_NAME            "Anonymous"
    BUNDLE_ID               "audio.chrona.chrona"
    PLUGIN_MANUFACTURER_CODE Anon
    PLUGIN_CODE             Chrn
    FORMATS                 ${CHRONA_FORMATS}
    PRODUCT_NAME            "CHRONA"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE          # MIDI trigger system
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${CHRONA_COPY_AFTER_BUILD}
    VST3_CATEGORIES         "Fx" "Delay" "Modulation"
    AU_MAIN_TYPE            "kAudioUnitType_Effect"
)

# ---------------------------------------------------------------------------
#  Sources (organised by module — see Source/ subfolders)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CHRONA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(CHRONA PRIVATE ${CHRONA_SOURCES})

target_include_directories(CHRONA PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

# Group in IDEs to mirror the on-disk module layout.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${CHRONA_SOURCES})

# ---------------------------------------------------------------------------
#  JUCE module config
# ---------------------------------------------------------------------------
target_compile_definitions(CHRONA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

# LTO gives smaller/faster binaries for release, but the link is slow — CI
# turns it off for quick feedback (-DCHRONA_LTO=OFF).
option(CHRONA_LTO "Enable link-time optimisation" ON)
if(CHRONA_LTO)
    target_link_libraries(CHRONA PUBLIC juce::juce_recommended_lto_flags)
endif()

target_link_libraries(CHRONA PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
#  Offline stress test (headless). Opt-in: -DCHRONA_BUILD_TESTS=ON
#  Add -DCHRONA_SANITIZE=ON for ASan+UBSan.
# ---------------------------------------------------------------------------
option(CHRONA_BUILD_TESTS "Build the offline stress test" OFF)
option(CHRONA_SANITIZE    "Build tests with ASan+UBSan"   OFF)
option(CHRONA_TSAN        "Build tests with ThreadSanitizer" OFF)

if(CHRONA_BUILD_TESTS)
    juce_add_console_app(ChronaTests PRODUCT_NAME "ChronaTests")
    target_sources(ChronaTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/StandaloneTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/params/Parameters.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/presets/PresetManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/TemporalEngine.cpp")
    target_include_directories(ChronaTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_compile_definitions(ChronaTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISABLE_JUCE_VERSION_PRINTING=1)
    target_link_libraries(ChronaTests PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if(CHRONA_SANITIZE)
        target_compile_options (ChronaTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
        target_link_options    (ChronaTests PRIVATE -fsanitize=address,undefined)
    endif()
    if(CHRONA_TSAN)
        target_compile_options (ChronaTests PRIVATE -fsanitize=thread -fno-omit-frame-pointer -O1 -g)
        target_link_options    (ChronaTests PRIVATE -fsanitize=thread)
    endif()
endif()

# ---------------------------------------------------------------------------
#  pluginval — industry-standard plug-in validator (state, threads, params,
#  fuzzing). Run:  cmake --build build --target validate
#  It uses an installed pluginval (PATH or -DPLUGINVAL_EXE=…) or downloads the
#  right prebuilt binary for the host OS. Override strictness with
#  -DPLUGINVAL_LEVEL=N and pass extra flags with -DPLUGINVAL_ARGS="--skip-gui-tests".
# ---------------------------------------------------------------------------
if(NOT PLUGINVAL_LEVEL)
    set(PLUGINVAL_LEVEL 10)
endif()
set(CHRONA_VST3_BUNDLE "${CMAKE_BINARY_DIR}/CHRONA_artefacts/$<CONFIG>/VST3/CHRONA.vst3")
add_custom_target(validate
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_PATH=${CHRONA_VST3_BUNDLE}
        -DPLUGINVAL_LEVEL=${PLUGINVAL_LEVEL}
        -DPLUGINVAL_EXE=${PLUGINVAL_EXE}
        -DPLUGINVAL_ARGS=${PLUGINVAL_ARGS}
        -DPLUGINVAL_DIR=${CMAKE_BINARY_DIR}/pluginval
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunPluginval.cmake"
    DEPENDS CHRONA_VST3
    VERBATIM
    USES_TERMINAL
    COMMENT "Validating CHRONA.vst3 with pluginval")

# BinaryData (factory presets, fonts) — optional, only if the folder exists.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
    file(GLOB CHRONA_RES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*")
    if(CHRONA_RES)
        juce_add_binary_data(ChronaData SOURCES ${CHRONA_RES})
        target_link_libraries(CHRONA PRIVATE ChronaData)
    endif()
endif()
