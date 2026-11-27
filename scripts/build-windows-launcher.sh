#!/usr/bin/env bash
# Build and package portable Windows x64 launch tools without disc inputs.
set -euo pipefail
cd "$(dirname "$0")/.."

build=build/windows-x64/runtime
mkdir -p build
stage=$(mktemp -d "$PWD/build/windows-launcher.XXXXXX")
trap 'rm -rf "$stage"' EXIT
package=$stage/Turbulence
mkdir -p "$package/toolchain/Source/Core/Core/PowerPC" "$package/toolchain/GXRuntime/src"
dolphin=lib/ModernGekko/vendor/dolphin
manifest="$PWD/ports/windows/runtime.manifest"

bash scripts/apply-patches.sh
cmake -S lib/ModernGekko -B "$build" -G Ninja \
  "-DCMAKE_TOOLCHAIN_FILE=$PWD/ports/windows/clang-cl-xwin.cmake" -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_EXE_LINKER_FLAGS_RELEASE=/INCREMENTAL:NO /MANIFEST:EMBED /MANIFESTINPUT:\"$manifest\"" \
  -DBUILD_TESTING=OFF -DUSE_SYSTEM_LIBS=OFF -DDOLRECOMP_ENABLE_LLVM=OFF \
  "-DTURBULENCE_OPTIONS_SOURCE_DIR=$PWD/src"
cmake --build "$build" --target moderngekko-run dolrecomp -j "${JOBS:-8}"

"${WINDOWS_CXX:-x86_64-w64-mingw32-g++}" -std=c++17 -O2 -Wall -Wextra -municode -mwindows \
  -static -static-libgcc -static-libstdc++ ports/windows/launcher.cpp -o "$package/Turbulence.exe" \
  -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32
cp "$build/moderngekko-run.exe" "$build/vendor/dolphin/DolRecomp/dolrecomp.exe" "$package/"
cp -R "$build/Sys" "$package/Sys"
cp -R "$dolphin/module-template" "$package/toolchain/module-template"
cp -R "$dolphin/GXRuntime/include" "$package/toolchain/GXRuntime/include"
cp -R "$dolphin/GXRuntime/src/core" "$package/toolchain/GXRuntime/src/core"
cp -R "$dolphin/Source/Core/Core/PowerPC/StaticRecomp" "$package/toolchain/Source/Core/Core/PowerPC/"
cp ports/windows/setup-game.py ports/windows/README.txt ports/extract-sonic-riders.py \
  config/GCPadNew.ini LICENSE "$package/"
find "$package" -name .DS_Store -type f -delete
(
  cd "$package"
  find . -type f -print | LC_ALL=C sort | while IFS= read -r file; do shasum -a 256 "$file"; done | \
    shasum -a 256 | cut -d ' ' -f 1
) > "$stage/build-id.txt"
mv "$stage/build-id.txt" "$package/"

mkdir -p build/release
output="$PWD/build/release/Turbulence-Windows-x64-Preview.zip"
(cd "$stage" && zip -qr launcher.zip Turbulence)
unzip -tq "$stage/launcher.zip"
mv -f "$stage/launcher.zip" "$output"
shasum -a 256 "$output"
