#!/usr/bin/env bash
# Build the Swift macOS launcher and assemble a self-contained Turbulence.app
# that carries the recompiler tools, the runtime's Sys directory, the
# module-compilation toolchain, and every non-system dylib those tools need,
# so a first run can turn a dropped ISO into a native game build on a clean
# Mac (no Homebrew required at runtime). No game code or data is included.
set -euo pipefail
cd "$(dirname "$0")/.."

CONFIG="${CONFIG:-release}"
NOLLVM="${TURBULENCE_LAUNCHER_RECOMPILER:-build/dolrecomp-nollvm/dolrecomp}"
RUNTIME_BUILD="${TURBULENCE_LAUNCHER_RUNTIME_BUILD:-build/codex-runtime}"
PORT="$RUNTIME_BUILD/moderngekko-port"
RUNNER="$RUNTIME_BUILD/moderngekko-run"
SYS="$RUNTIME_BUILD/Sys"
DOLPHIN=lib/ModernGekko/vendor/dolphin

# A dolrecomp without the LLVM backend avoids a runtime dependency on the
# player's Homebrew LLVM (whose ABI drifts across versions).
if [ ! -x "$NOLLVM" ]; then
  if [ "$NOLLVM" != build/dolrecomp-nollvm/dolrecomp ]; then
    echo "error: missing launcher recompiler $NOLLVM" >&2
    exit 1
  fi
  echo "Building a C-backend-only dolrecomp (no LLVM)..."
  cmake -S lib/DolRecomp -B build/dolrecomp-nollvm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=OFF >/dev/null
  cmake --build build/dolrecomp-nollvm --target dolrecomp -j"$(sysctl -n hw.ncpu)" >/dev/null
fi

for bin in "$NOLLVM" "$PORT" "$RUNNER"; do
  [ -x "$bin" ] || { echo "error: missing tool $bin (run 'make tools' first)" >&2; exit 1; }
done

echo "Building Turbulence launcher ($CONFIG)..."
swift build -c "$CONFIG" --package-path launcher

APP=build/release/Turbulence.app
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/toolchain/vendor/dolphin" \
  "$APP/Contents/Frameworks"

# Executables (siblings: moderngekko-port looks for dolrecomp and moderngekko-run beside itself).
cp launcher/.build/$CONFIG/TurbulenceLauncher "$APP/Contents/MacOS/Turbulence"
cp "$NOLLVM" "$APP/Contents/MacOS/dolrecomp"
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

# Copy every non-system dylib the tools need into Frameworks and point each
# binary (and each copied dylib) at @executable_path/../Frameworks.
FRAMEWORKS="$APP/Contents/Frameworks"
bundle_dylibs() {
  local target="$1"
  local dep name
  while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    name="$(basename "$dep")"
    if [ ! -e "$FRAMEWORKS/$name" ]; then
      cp -L "$dep" "$FRAMEWORKS/$name" 2>/dev/null || continue
      chmod +w "$FRAMEWORKS/$name" 2>/dev/null || true
      bundle_dylibs "$FRAMEWORKS/$name"
    fi
    install_name_tool -change "$dep" "@executable_path/../Frameworks/$name" "$target" 2>/dev/null || true
  done < <(otool -L "$target" | tail -n +2 | sed -E 's/^[[:space:]]*//; s/ \(.*//' | grep -E '^/opt/homebrew|^/usr/local')
}
bundle_dylibs "$APP/Contents/MacOS/moderngekko-port"
bundle_dylibs "$APP/Contents/MacOS/moderngekko-run"
bundle_dylibs "$APP/Contents/MacOS/dolrecomp"

# Some Homebrew dylibs reference each other via @rpath; give the tools an
# rpath into Frameworks so those resolve without Homebrew.
for bin in "$APP/Contents/MacOS/moderngekko-port" "$APP/Contents/MacOS/moderngekko-run"; do
  install_name_tool -add_rpath "@executable_path/../Frameworks" "$bin" 2>/dev/null || true
done

# Ad-hoc sign so it runs locally without a Developer ID.
codesign --force --deep --sign - "$APP" 2>/dev/null || true

echo ""
echo "Built $APP"
echo "Run: open '$APP'"
