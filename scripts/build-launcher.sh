#!/usr/bin/env bash
# Build the Swift macOS launcher and assemble a self-contained Turbulence.app
# that carries the recompiler tools, the runtime's Sys directory, and the
# module-compilation toolchain, so a first run can turn a dropped ISO into a
# native game build. No game code or data is included.
set -euo pipefail
cd "$(dirname "$0")/.."

CONFIG="${CONFIG:-release}"
DOLRECOMP=build/dolrecomp/dolrecomp
PORT=build/codex-runtime/moderngekko-port
RUNNER=build/codex-runtime/moderngekko-run
SYS=build/codex-runtime/Sys
DOLPHIN=lib/ModernGekko/vendor/dolphin

for bin in "$DOLRECOMP" "$PORT" "$RUNNER"; do
  [ -x "$bin" ] || { echo "error: missing tool $bin (run 'make tools' first)" >&2; exit 1; }
done

echo "Building Turbulence launcher ($CONFIG)..."
swift build -c "$CONFIG" --package-path launcher

APP=build/release/Turbulence.app
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/toolchain/vendor/dolphin"

# Executables (siblings: moderngekko-port looks for dolrecomp and moderngekko-run beside itself).
cp launcher/.build/$CONFIG/TurbulenceLauncher "$APP/Contents/MacOS/Turbulence"
cp "$DOLRECOMP" "$APP/Contents/MacOS/dolrecomp"
cp "$PORT" "$APP/Contents/MacOS/moderngekko-port"
cp "$RUNNER" "$APP/Contents/MacOS/moderngekko-run"
cp -R "$SYS" "$APP/Contents/MacOS/Sys"

# Recompile toolchain (the module sources moderngekko-port compiles against).
TC="$APP/Contents/Resources/toolchain/vendor/dolphin"
cp -R "$DOLPHIN/module-template" "$TC/module-template"
mkdir -p "$TC/GXRuntime/src" "$TC/Source/Core/Core/PowerPC"
cp -R "$DOLPHIN/GXRuntime/include" "$TC/GXRuntime/include"
cp -R "$DOLPHIN/GXRuntime/src/core" "$TC/GXRuntime/src/core"
cp -R "$DOLPHIN/Source/Core/Core/PowerPC/StaticRecomp" "$TC/Source/Core/Core/PowerPC/StaticRecomp"
mkdir -p "$TC/Data/Sys/GameSettings"
cp "$DOLPHIN/Data/Sys/GameSettings/GXEE8P.ini" "$TC/Data/Sys/GameSettings/GXEE8P.ini"

# Default settings + bundle metadata.
mkdir -p "$APP/Contents/Resources/config"
cp config/GFX.ini "$APP/Contents/Resources/config/GFX.ini"
cp launcher/Info.plist "$APP/Contents/Info.plist"

# Ad-hoc sign so it runs locally without a Developer ID.
codesign --force --deep --sign - "$APP" 2>/dev/null || true

echo ""
echo "Built $APP"
echo "Run: open '$APP'"
