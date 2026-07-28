#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  macOS packaging: codesign + notarize the VST3/AU, then build a .pkg.
#  No-op-signs gracefully when signing secrets are absent (local dev / CI fork).
#
#  Required env for a fully signed+notarized build:
#    MACOS_CERT_ID   "Developer ID Application: Your Name (TEAMID)"
#    MACOS_INST_ID   "Developer ID Installer: Your Name (TEAMID)"
#    AC_PROFILE      notarytool keychain profile name (xcrun notarytool store-credentials)
#  Usage: scripts/package-macos.sh <build-dir>
# ---------------------------------------------------------------------------
set -euo pipefail
BUILD="${1:-build}"
ART="$BUILD/CHRONA_artefacts/Release"
VST3="$ART/VST3/CHRONA.vst3"
AU="$ART/AU/CHRONA.component"

sign() {  # sign a bundle if a cert is configured
  local target="$1"
  [ -e "$target" ] || return 0
  if [ -n "${MACOS_CERT_ID:-}" ]; then
    echo ">> codesign $target"
    codesign --force --deep --options runtime --timestamp \
      --sign "$MACOS_CERT_ID" "$target"
  else
    echo "!! MACOS_CERT_ID unset — skipping codesign of $target"
  fi
}

sign "$VST3"
sign "$AU"

# Build a component-style installer package.
PKG="CHRONA-macOS.pkg"
ROOT="$(mktemp -d)"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components"
[ -e "$VST3" ] && cp -R "$VST3" "$ROOT/Library/Audio/Plug-Ins/VST3/"
[ -e "$AU" ]   && cp -R "$AU"   "$ROOT/Library/Audio/Plug-Ins/Components/"

if [ -n "${MACOS_INST_ID:-}" ]; then
  pkgbuild --root "$ROOT" --identifier com.artifacts.chrona.pkg --version "${VERSION:-1.0.0}" \
           --install-location / --sign "$MACOS_INST_ID" "$PKG"
else
  echo "!! MACOS_INSTALLER_ID unset — building unsigned pkg"
  pkgbuild --root "$ROOT" --identifier com.artifacts.chrona.pkg --version "${VERSION:-1.0.0}" \
           --install-location / "$PKG"
fi

# Notarize + staple when a notarytool profile is available.
if [ -n "${AC_PROFILE:-}" ]; then
  echo ">> notarize $PKG"
  xcrun notarytool submit "$PKG" --keychain-profile "$AC_PROFILE" --wait
  xcrun stapler staple "$PKG"
else
  echo "!! AC_NOTARY_PROFILE unset — skipping notarization"
fi

echo ">> done: $PKG"
