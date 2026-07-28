#!/usr/bin/env bash
# Copy the CHRONA factory preset banks into the user preset directory.
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)/presets"
case "$(uname -s)" in
  Darwin) DEST="$HOME/Library/Application Support/CHRONA/Presets" ;;
  *)      DEST="${XDG_CONFIG_HOME:-$HOME/.config}/CHRONA/Presets" ;;
esac
mkdir -p "$DEST"
# copy bank folders (skip the README)
for d in "$SRC"/*/; do cp -R "$d" "$DEST/"; done
echo "Installed CHRONA preset banks to: $DEST"
