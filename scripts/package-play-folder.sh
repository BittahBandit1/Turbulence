#!/bin/bash
# Refresh a "play folder" (bin/, module/, extracted/, user/, launcher) from this
# repo's current build. The previous bin/, module/ and launcher are moved to
# previous-<timestamp>/ inside the folder; extracted/ and user/ (saves,
# settings) are left alone.
#   scripts/package-play-folder.sh "/Volumes/1TB TG/ROMS/GCN/Turbulence"
set -eu
cd "$(dirname "$0")/.."
DEST=${1:?usage: package-play-folder.sh <play folder>}
RUN=lib/ModernGekko/build/moderngekko-run
ACTIVE=$(sed -n 's/^module=//p;t;p' build/modules/GXEE8P/active-module.txt | head -1)
[ -f "$ACTIVE" ] || ACTIVE=$(ls -t build/modules/GXEE8P/*/gGXEE8P_recomp.dylib | head -1)
[ -x "$RUN" ] && [ -f "$ACTIVE" ] || { echo "build first (make run GAME=...)"; exit 1; }
mkdir -p "$DEST"
backup="$DEST/previous-$(date +%Y%m%d-%H%M%S)"
for item in bin module launch_sonic_riders.sh; do
  if [ -e "$DEST/$item" ]; then mkdir -p "$backup"; mv "$DEST/$item" "$backup/"; fi
done
mkdir -p "$DEST/bin" "$DEST/module" "$DEST/config" "$DEST/user/Config"
cp "$RUN" "$DEST/bin/"
cp -R lib/ModernGekko/build/Sys "$DEST/bin/Sys"
cp "$ACTIVE" "$DEST/module/gGXEE8P_recomp.dylib"
cp config/GCPadNew.ini "$DEST/config/"
cat > "$DEST/launch_sonic_riders.sh" <<'LAUNCH'
#!/usr/bin/env bash
# Turbulence - Sonic Riders (GameCube) static recompilation for macOS.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$ROOT_DIR/user/Config"
[ -f "$ROOT_DIR/user/Config/GCPadNew.ini" ] || cp "$ROOT_DIR/config/GCPadNew.ini" "$ROOT_DIR/user/Config/"
# The options menu (R1 / Tab) can toggle the Metal HUD; this only enables the
# machinery. Its On/Off choice is saved in user/Config/Turbulence.ini.
export MTL_HUD_ENABLED="${MTL_HUD_ENABLED:-1}"
echo "Turbulence - Sonic Riders | options: R1 / Tab | fullscreen: Cmd+Return"
exec "$ROOT_DIR/bin/moderngekko-run" \
  --game "$ROOT_DIR/extracted/Sonic-Riders-USA-En-Ja-Fr-De-Es-It" \
  --module "$ROOT_DIR/module/gGXEE8P_recomp.dylib" \
  --user-dir "$ROOT_DIR/user" \
  --graphics Metal \
  --title "Turbulence - Sonic Riders" \
  "$@"
LAUNCH
chmod +x "$DEST/launch_sonic_riders.sh"
echo "updated $DEST (previous files: ${backup#$DEST/})"
