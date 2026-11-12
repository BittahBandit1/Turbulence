#!/bin/bash
# Cross-build the Windows x64 runtime and game module from macOS (clang-cl +
# xwin, see ports/windows/clang-cl-xwin.cmake) and zip a test folder.
# Needs LLVM objects in build/llvm-windows-x86_64 and the C REL output.
set -euo pipefail
cd "$(dirname "$0")/.."
TC="$PWD/ports/windows/clang-cl-xwin.cmake"
B=build/windows-x64
REL=$(ls -d build/modules/GXEE8P/*-5f8c8e46802d6f00/dolrecomp-output/rel/generated | head -1)
cmake -S lib/ModernGekko -B $B/runtime -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DTURBULENCE_OPTIONS_SOURCE_DIR="$PWD/src" \
  -DUSE_SYSTEM_LIBS=OFF -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local"
cmake --build $B/runtime --target moderngekko-run -j"${JOBS:-10}" ${NINJA_K:+-- -k 0}
cmake -S lib/ModernGekko/vendor/dolphin/module-template -B $B/module -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TC" \
  -DCMAKE_BUILD_TYPE=Release -DGAME_ID=GXEE8P -DGENERATED_DIR="$PWD/build/llvm-windows-x86_64/dol/generated" \
  -DREL_GENERATED_DIR="$PWD/$REL" -DGXRUNTIME_DIR="$PWD/lib/ModernGekko/vendor/dolphin/GXRuntime" \
  -DCHASSIS_ABI_DIR="$PWD/lib/ModernGekko/vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp"
cmake --build $B/module -j"${JOBS:-10}"
P=$B/Turbulence-Windows-x64; rm -rf $P; mkdir -p $P/module
cp $B/runtime/moderngekko-run.exe $P/; cp -R $B/runtime/Sys $P/Sys
cp $B/module/gGXEE8P_recomp.dll $P/module/; cp ports/windows/Turbulence.bat config/GCPadNew.ini ports/extract-sonic-riders.py $P/
(cd $B && rm -f ../release/Turbulence-Windows-x64.zip && zip -qr ../release/Turbulence-Windows-x64.zip Turbulence-Windows-x64)
ls -la build/release/Turbulence-Windows-x64.zip
