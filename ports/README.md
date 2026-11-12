# Turbulence ports

The Android and iOS hosts are adapted from Crazy Taxi ReFare's mobile ports
(ReFare contributors, GPL-3.0; see COPYING). Turbulence keeps its own disc ID,
DOL/REL validation, static game modules, options, prompts and Sonic Riders touch
controls. Runtime adaptations retain their existing SPDX notices.

Both apps import a personal Sonic Riders USA revision 0 ISO, GCM or GameCube
CISO. The APK and IPA include generated native code and runtime system files;
they do not include the game's extracted assets or a disc image. Both main.dol
and _Main.rel must match the compiled version. Import is transactional and
rejects traversal, truncation, wrong revisions and replacement of a library.

## Build

Generate the personal main.dol and REL C outputs through the existing project
build first. The scripts find matching outputs in build/modules/GXEE8P, or
accept explicit --generated and --rel-generated directories. From a fresh
checkout, initialize the submodules and run scripts/apply-patches.sh first.

Android requires JDK, SDK platform/build tools 35, and NDK 27.3.13750724:

```sh
python3 scripts/android.py --sdk "$ANDROID_HOME"
```

main.dol is built with DolRecomp's LLVM backend by default (the REL stays C);
pass `--c-backend` to either script for the older C output. LLVM output needs
DolRecomp built with LLVM 19/20 in lib/DolRecomp/build-llvm.

The debug-signed ARM64 APK is written under build/release. A local signing key
is generated only in build/android-tools. Minimum Android version is 8.0/API26.
Touch controls include an analog pad, jump A, boost B, independent L/R drift,
and Start. A three-finger tap opens options; controllers are also supported.

Physical iPhone/iPad requires macOS and Xcode with the iPhoneOS SDK:

```sh
python3 scripts/ios.py gameplay-app --ipa
```

The ARM64 IPA under build/release targets iOS 15 and newer. It is unsigned and
must be signed by a personal sideloading/development workflow before device
installation. It statically links both game modules, uses Metal and native iOS
audio, and includes touch/game-controller input. Runtime compilation and an
executable JIT are disabled; unmatched guest code uses interpreter fallback.
The scripts verify device Mach-O architecture and minimum OS before packaging.

Game controller family changes update automatic prompts; manual choices in
Turbulence's options take priority. Apps pause input/audio when backgrounded.
Desktop settings are separate; mobile starts at native internal resolution.

Android on Adreno GPUs can load a custom Vulkan driver (for example Mesa
Turnip): Install GPU driver (.zip) on the home screen takes a standard
adrenotools package (meta.json plus the driver library). Use system driver
removes it. The first launch with a new driver compiles shaders for it.

## Windows x64 and Linux Flatpak

These are cross-built from macOS. Both use LLVM main.dol objects
(x86-64-v2 with a runtime-selected x86-64-v3 copy on x86_64) and the C REL.

```sh
# Windows: needs Homebrew llvm@20 + lld and the MSVC CRT/SDK from xwin in ~/.xwin
#   (xwin --accept-license splat --output ~/.xwin)
bash scripts/windows.sh          # -> build/release/Turbulence-Windows-x64.zip
# Linux: needs Docker (Colima with --vz-rosetta for x86_64 on Apple Silicon)
bash scripts/flatpak.sh aarch64  # -> build/release/Turbulence-linux-aarch64.flatpak
bash scripts/flatpak.sh x86_64   # -> build/release/Turbulence-linux-x86_64.flatpak
```

The LLVM objects come from
`lib/DolRecomp/build-llvm/dolrecomp --backend=llvm --cpu gekko --targets <list>`
with `DOLRECOMP_LLVM_TARGET` set to `x86_64-pc-windows-msvc`,
`x86_64-unknown-linux-gnu` or `aarch64-unknown-linux-gnu`, written to
build/llvm-<windows-x86_64|linux-x86_64|linux-aarch64>/dol/generated.

Windows: unzip, put the extracted Sonic Riders USA folder (with sys\main.dol)
in `game` next to Turbulence.bat, and run Turbulence.bat. Settings go in `user`.

Linux: `flatpak install --user Turbulence-linux-<arch>.flatpak` (pulls the
freedesktop 25.08 runtime from Flathub), put the extracted game in
~/Turbulence/game (or set TURBULENCE_GAME), then
`flatpak run io.github.bittahbandit1.Turbulence`.

## Verification

Tests: tests/android_import_test.py, tests/ios_import_test.swift,
tests/mobile_touch_test.swift, and tests/mobile_build_test.py. The importer
checks run with synthetic images and do not need a game disc. Physical-device
playback and thermal/frame-pacing behavior must be checked separately from
successful cross-compilation and package validation.
