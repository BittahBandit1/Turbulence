#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_ROOT="$ROOT_DIR/extracted/Sonic-Riders-USA-En-Ja-Fr-De-Es-It"
MODULE_PATH="$ROOT_DIR/module/gGXEE8P_recomp.dylib"
USER_DIR="$ROOT_DIR/user"
RUNNER="$ROOT_DIR/bin/moderngekko-run"

mkdir -p "$USER_DIR/Config"
cp -f "$ROOT_DIR/config/GCPadNew.ini" "$USER_DIR/Config/GCPadNew.ini"

# Ensure BackgroundInput is disabled and audio backend is Cubeb in Dolphin.ini
if [ -f "$USER_DIR/Config/Dolphin.ini" ]; then
  sed -i '' 's/BackgroundInput = True/BackgroundInput = False/g' "$USER_DIR/Config/Dolphin.ini" 2>/dev/null || true
  sed -i '' 's/Backend = No Audio Output/Backend = Cubeb/g' "$USER_DIR/Config/Dolphin.ini" 2>/dev/null || true
fi

# Remove any legacy config.ini that forced Vulkan so Metal is used by default
if [ -f "$USER_DIR/config.ini" ] && grep -q "backend = Vulkan" "$USER_DIR/config.ini" 2>/dev/null; then
  rm -f "$USER_DIR/config.ini"
fi

# Enable Apple Metal Performance HUD by default (can be toggled live in the R1 menu)
export MTL_HUD_ENABLED="${MTL_HUD_ENABLED:-1}"

# Fallback Vulkan paths if user explicitly passes --graphics Vulkan
if [ -f "$ROOT_DIR/lib/libMoltenVK.dylib" ]; then
  export LIBVULKAN_PATH="$ROOT_DIR/lib/libMoltenVK.dylib"
  export DYLD_FALLBACK_LIBRARY_PATH="$ROOT_DIR/lib:/opt/homebrew/lib:${DYLD_FALLBACK_LIBRARY_PATH:-}"
elif [ -f "/opt/homebrew/lib/libMoltenVK.dylib" ]; then
  export LIBVULKAN_PATH="/opt/homebrew/lib/libMoltenVK.dylib"
fi

cat <<'BANNER'
==========================================
 Launching Sonic Riders macOS Recomp
 Game ID: GXEE8P (GameCube)
 Backend: Portable C + Native Metal
 Controls: Press [R1] or [Tab] for Options
==========================================
BANNER

exec "$RUNNER" \
  --game "$GAME_ROOT" \
  --module "$MODULE_PATH" \
  --user-dir "$USER_DIR" \
  --graphics Metal \
  --title "Turbulence - Sonic Riders (macOS Static Recomp)" \
  "$@"
