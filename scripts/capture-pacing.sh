#!/usr/bin/env bash
# Launch normal play and collect three 600-frame samples while the game is visible.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CAPTURE_DIR="$ROOT_DIR/build/pacing-live/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$CAPTURE_DIR"
printf '%s\n' "$CAPTURE_DIR" > "$ROOT_DIR/build/pacing-live/latest.txt"
printf '%s\n' "$$" > "$CAPTURE_DIR/game.pid"
if [ "${1:-}" = "--hud" ]; then
  shift
  export TURBULENCE_DUMP_UI=1
  export TURBULENCE_UI_DUMP_DIR="$CAPTURE_DIR/atlases"
  mkdir -p "$TURBULENCE_UI_DUMP_DIR/raw"
  echo "HUD texture collection enabled for identifying the remaining controller graphics."
fi
exec > >(tee "$CAPTURE_DIR/launch.log") 2>&1
echo "Pacing captures will start about 20, 40 and 60 seconds after launch."
echo "Keep Turbulence visible and play normally for at least 75 seconds."
echo "Capture folder: $CAPTURE_DIR"
GAME_PID=$$
(
  for sample in 1 2 3; do
    sleep 20
    kill -0 "$GAME_PID" 2>/dev/null || exit 0
    touch "$CAPTURE_DIR/pacing-request"
  done
) &
export TURBULENCE_PACING_DIR="$CAPTURE_DIR"
exec "$ROOT_DIR/launch_sonic_riders.sh" "$@"
