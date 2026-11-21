#!/bin/sh
# Turbulence launcher (Flatpak). Point TURBULENCE_GAME at an extracted Sonic
# Riders (USA) folder (it contains sys/main.dol); ~/Turbulence/game by default.
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/turbulence"
GAME="${TURBULENCE_GAME:-$HOME/Turbulence/game}"
mkdir -p "$DATA/Config"
# Handheld input daemons (InputPlumber on ArmadaOS/ChimeraOS/Bazzite, Steam) expose
# the built-in pad as a Valve "Steam Virtual Gamepad" (28de:11ff). SDL3 hides that
# device unless this is set, so the game would see no controller at all.
export SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD="${SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD:-1}"
[ -f "$DATA/Config/GCPadNew.ini" ] || cp /app/share/turbulence/GCPadNew.ini "$DATA/Config/"
if [ ! -f "$GAME/sys/main.dol" ]; then
  echo "Turbulence: no extracted game at $GAME (expected $GAME/sys/main.dol)." >&2
  echo "Copy your extracted Sonic Riders (USA) folder there, or run:" >&2
  echo "  flatpak run --env=TURBULENCE_GAME=/path/to/game io.github.bittahbandit1.Turbulence" >&2
  exit 1
fi
exec /app/bin/moderngekko-run --game "$GAME" --module /app/lib/turbulence/gGXEE8P_recomp.so \
  --user-dir "$DATA" "$@"
