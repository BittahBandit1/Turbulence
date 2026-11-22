#!/usr/bin/env bash
# Build the Swift macOS launcher and assemble Turbulence.app under build/release.
set -euo pipefail
cd "$(dirname "$0")/.."

CONFIG="${CONFIG:-release}"
echo "Building Turbulence launcher ($CONFIG)..."

swift build -c "$CONFIG" --package-path launcher

BIN="launcher/.build/$CONFIG/TurbulenceLauncher"
APP="build/release/Turbulence.app"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp "$BIN" "$APP/Contents/MacOS/Turbulence"
cp launcher/Info.plist "$APP/Contents/Info.plist"

# Ad-hoc sign so it runs locally without a Developer ID. A real release build
# would re-sign with a Developer ID identity for distribution.
codesign --force --deep --sign - "$APP" 2>/dev/null || true

echo ""
echo "Built $APP"
echo "Run: open '$APP'"
