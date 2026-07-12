#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  CHRONA — one-shot build for macOS and Linux.
#
#  Just run:   ./build.sh            (Release)
#              ./build.sh Debug
#
#  It checks your toolchain, installs the Linux dev libraries JUCE needs
#  (apt / dnf / pacman / zypper — skipped on macOS), fetches JUCE via CMake on
#  the first run, then configures and builds. Set CHRONA_SKIP_DEPS=1 to skip
#  the dependency install. Pass extra CMake flags in CHRONA_CMAKE_ARGS.
# ---------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

say()  { printf '\033[1;36m>> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m!! %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31mXX %s\033[0m\n' "$*" >&2; exit 1; }

# --- toolchain checks ------------------------------------------------------
command -v cmake >/dev/null 2>&1 || die "CMake not found. Install CMake >= 3.22 (https://cmake.org/download)."
CMAKE_VER="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?')"
say "CMake $CMAKE_VER"

OS="$(uname -s)"

# --- Linux dependencies ----------------------------------------------------
install_linux_deps() {
    [ "${CHRONA_SKIP_DEPS:-0}" = "1" ] && { warn "CHRONA_SKIP_DEPS=1 — skipping dependency install."; return; }

    local SUDO=""
    if [ "$(id -u)" -ne 0 ]; then
        command -v sudo >/dev/null 2>&1 && SUDO="sudo" || { warn "not root and no sudo — skipping deps (install them yourself if configure fails)."; return; }
    fi

    if command -v apt-get >/dev/null 2>&1; then
        say "Installing build deps via apt…"
        $SUDO apt-get update -qq
        $SUDO apt-get install -y --no-install-recommends \
            build-essential pkg-config \
            libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
            libxcursor-dev libxrender-dev libfreetype6-dev libfontconfig1-dev
    elif command -v dnf >/dev/null 2>&1; then
        say "Installing build deps via dnf…"
        $SUDO dnf install -y \
            gcc-c++ pkgconf-pkg-config \
            alsa-lib-devel libX11-devel libXext-devel libXrandr-devel libXinerama-devel \
            libXcursor-devel libXrender-devel freetype-devel fontconfig-devel
    elif command -v pacman >/dev/null 2>&1; then
        say "Installing build deps via pacman…"
        $SUDO pacman -S --needed --noconfirm \
            base-devel pkgconf alsa-lib libx11 libxext libxrandr libxinerama \
            libxcursor libxrender freetype2 fontconfig
    elif command -v zypper >/dev/null 2>&1; then
        say "Installing build deps via zypper…"
        $SUDO zypper install -y \
            gcc-c++ pkg-config \
            alsa-devel libX11-devel libXext-devel libXrandr-devel libXinerama-devel \
            libXcursor-devel libXrender-devel freetype2-devel fontconfig-devel
    else
        warn "Unknown package manager — if configure fails, install the JUCE Linux deps manually (see README)."
    fi
}

if [ "$OS" = "Linux" ]; then
    install_linux_deps
elif [ "$OS" = "Darwin" ]; then
    xcode-select -p >/dev/null 2>&1 || die "Xcode Command Line Tools missing. Run: xcode-select --install"
    say "macOS toolchain OK"
fi

# --- configure + build -----------------------------------------------------
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

say "Configuring ($BUILD_TYPE)…  (JUCE is fetched automatically on first run)"
# shellcheck disable=SC2086
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ${CHRONA_CMAKE_ARGS:-}

say "Building with $JOBS jobs…"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$JOBS"

echo
say "Done. Plugin bundles:"
find "$BUILD_DIR" \( -name "*.vst3" -o -name "*.component" -o -name "*.app" \) -maxdepth 4 2>/dev/null | sed 's/^/     /' || true
echo
say "On macOS these are also installed to ~/Library/Audio/Plug-Ins."
say "Rescan in your DAW and load CHRONA (manufacturer: Anonymous)."
