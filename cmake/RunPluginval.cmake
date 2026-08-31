# ============================================================================
#  RunPluginval.cmake — locate (or download) pluginval and validate the plug-in.
#
#  Invoked by the `validate` build target:
#      cmake --build build --target validate
#
#  Inputs (passed with -D by the target, or overridable on the command line):
#    PLUGIN_PATH     full path to the built .vst3 bundle
#    PLUGINVAL_LEVEL strictness 1..10 (default 10)
#    PLUGINVAL_EXE   path to an existing pluginval binary (skips the download)
#    PLUGINVAL_ARGS  extra args, e.g. --skip-gui-tests (headless CI)
#    PLUGINVAL_DIR   where to cache a downloaded pluginval (default: build dir)
# ============================================================================

if(NOT DEFINED PLUGINVAL_LEVEL)
    set(PLUGINVAL_LEVEL 10)
endif()

# ---- 1. locate pluginval ---------------------------------------------------
set(_pv "")

if(DEFINED PLUGINVAL_EXE AND EXISTS "${PLUGINVAL_EXE}")
    set(_pv "${PLUGINVAL_EXE}")
else()
    find_program(_pv_found NAMES pluginval)
    if(_pv_found)
        set(_pv "${_pv_found}")
    endif()
endif()

# ---- 2. download a prebuilt binary if we still don't have one --------------
if(NOT _pv)
    if(NOT DEFINED PLUGINVAL_DIR)
        set(PLUGINVAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/pluginval")
    endif()

    if(APPLE)
        set(_asset "pluginval_macOS.zip")
        set(_exe_rel "pluginval.app/Contents/MacOS/pluginval")
    elseif(WIN32)
        set(_asset "pluginval_Windows.zip")
        set(_exe_rel "pluginval.exe")
    else()
        set(_asset "pluginval_Linux.zip")
        set(_exe_rel "pluginval")
    endif()

    set(_url "https://github.com/Tracktion/pluginval/releases/latest/download/${_asset}")
    set(_zip "${PLUGINVAL_DIR}/${_asset}")

    if(NOT EXISTS "${PLUGINVAL_DIR}/${_exe_rel}")
        message(STATUS "Downloading pluginval: ${_url}")
        file(MAKE_DIRECTORY "${PLUGINVAL_DIR}")
        file(DOWNLOAD "${_url}" "${_zip}" STATUS _dl SHOW_PROGRESS)
        list(GET _dl 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            list(GET _dl 1 _dl_msg)
            message(FATAL_ERROR
                "Could not download pluginval (${_dl_msg}).\n"
                "Install it manually and pass -DPLUGINVAL_EXE=/path/to/pluginval, "
                "or download from https://github.com/Tracktion/pluginval/releases")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${PLUGINVAL_DIR}")
    endif()

    set(_pv "${PLUGINVAL_DIR}/${_exe_rel}")
    if(UNIX)
        execute_process(COMMAND chmod +x "${_pv}")
    endif()
endif()

if(NOT EXISTS "${_pv}")
    message(FATAL_ERROR "pluginval not found at '${_pv}'")
endif()

# ---- 3. run it -------------------------------------------------------------
message(STATUS "pluginval: ${_pv}")
message(STATUS "validating: ${PLUGIN_PATH}  (strictness ${PLUGINVAL_LEVEL})")

separate_arguments(_extra UNIX_COMMAND "${PLUGINVAL_ARGS}")

execute_process(
    COMMAND "${_pv}" --strictness-level "${PLUGINVAL_LEVEL}" ${_extra} --validate "${PLUGIN_PATH}"
    RESULT_VARIABLE _rc)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "pluginval reported failures (exit ${_rc}).")
endif()
message(STATUS "pluginval: PASSED")
