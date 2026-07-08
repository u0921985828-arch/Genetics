#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  CHRONA — one-command build for macOS and Linux.
#
#  Requirements (installed once):
#    * CMake >= 3.22        (brew install cmake  /  apt install cmake)
#    * A C++17 compiler     (Xcode CLT on macOS, gcc/clang on Linux)
#    * Internet on first run (JUCE is fetched automatically by CMake)
#    * Linux only: sudo apt install libasound2-dev libx11-dev libxext-dev \
#                       libxrandr-dev libxinerama-dev libxcursor-dev \
#                       libfreetype6-dev libcurl4-openssl-dev pkg-config
#
#  Usage:  ./build.sh            (Release)
#          ./build.sh Debug
# ---------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="${1:-Release}"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo ">> Configuring ($BUILD_TYPE)…"
cmake -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo ">> Building with $JOBS jobs…"
cmake --build build --config "$BUILD_TYPE" -j "$JOBS"

echo ""
echo ">> Done. Plugin bundles are under:"
find build -name "*.vst3" -o -name "*.component" -o -name "*.app" 2>/dev/null | sed 's/^/     /' || true
echo ""
echo "   macOS installs to ~/Library/Audio/Plug-Ins automatically (COPY_PLUGIN_AFTER_BUILD)."
