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
    COMPANY_NAME            "Genetics Audio"
    BUNDLE_ID               "com.geneticsaudio.chrona"
    PLUGIN_MANUFACTURER_CODE Gntx
    PLUGIN_CODE             Chrn
    FORMATS                 ${CHRONA_FORMATS}
    PRODUCT_NAME            "CHRONA"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE          # MIDI trigger system
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
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

target_link_libraries(CHRONA PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# BinaryData (factory presets, fonts) — optional, only if the folder exists.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
    file(GLOB CHRONA_RES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*")
    if(CHRONA_RES)
        juce_add_binary_data(ChronaData SOURCES ${CHRONA_RES})
        target_link_libraries(CHRONA PRIVATE ChronaData)
    endif()
endif()
