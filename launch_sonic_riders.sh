#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_ROOT="$ROOT_DIR/extracted/Sonic-Riders-USA-En-Ja-Fr-De-Es-It"
MODULE_PATH="$ROOT_DIR/module/gGXEE8P_recomp.dylib"
USER_DIR="$ROOT_DIR/user"
RUNNER="$ROOT_DIR/bin/moderngekko-run"

# The source workspace uses its locally rebuilt runtime and existing module.
# Packaged play folders retain bin/, module/ and user/.
if [ ! -x "$RUNNER" ]; then
  RUNNER="$ROOT_DIR/build/codex-runtime/moderngekko-run"
  USER_DIR="$ROOT_DIR/build/user"
fi
if [ ! -f "$MODULE_PATH" ]; then
  MODULE_PATH="$ROOT_DIR/$(head -n 1 "$ROOT_DIR/build/modules/GXEE8P/active-module.txt")"
fi
if [ ! -x "$RUNNER" ] || [ ! -f "$MODULE_PATH" ]; then
  echo "Turbulence runtime or game module is missing. Build the project first." >&2
  exit 1
fi
mkdir -p "$USER_DIR/Config"
[ -f "$USER_DIR/Config/GCPadNew.ini" ] || cp "$ROOT_DIR/config/GCPadNew.ini" "$USER_DIR/Config/"

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
