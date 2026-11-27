# Turbulence — Sonic Riders (GameCube)

A native static recompilation of **Sonic Riders** (USA, `GXEE8P`) that runs on
macOS, Windows, Linux, Android, and iOS. Turbulence recompiles the game's
PowerPC code to native machine code at build time and runs it against a
Dolphin-derived runtime — full speed, no interpreter, and proper widescreen.

It is built on [DolRecomp](https://github.com/ExpansionPak/DolRecomp) (the static
recompiler) and [ModernGekko](https://github.com/ExpansionPak/ModernGekko) (the
runtime, built on Dolphin's video/audio/HLE core). Sonic Riders–specific changes
are kept as patches in `patches/` rather than forks.

> [!IMPORTANT]
> **No game data is included in or downloaded by this repository.** You must
> supply your own legally-obtained copy of *Sonic Riders* (USA, revision 0).
> This project is for preservation and interoperability only and is not
> affiliated with or endorsed by SEGA.

## Features

- **Full-game static recompilation.** `main.dol` uses DolRecomp's LLVM backend;
  `_Main.rel` (about two thirds of the game's code) is recompiled and linked at
  its runtime address. Every chunk is hash-verified against guest RAM before it
  runs, falling back to the JIT only if a chunk no longer matches.
- **60 FPS on desktop.** Locked frame pacing, persistent shader caches, and an
  idle-loop fast path for the SDK cache routines. Framerates beyond 60 will be added later.
- **Widescreen and ultrawide.** 2D draws are classified and stretched or centered
  appropriately; 3D uses Hor+/Vert+ projection with correct frustum updates, from
  4:3 through 32:9 and portrait.
- **Dynamic controller prompts.** GameCube, Xbox, and PlayStation button artwork
  follows the connected controller automatically, with a manual override.
- **In-game options menu** (R1 / Tab): aspect ratio, V-Sync, button-prompt style,
  FPS counter, and native Recomp Settings, styled to match the game.
- **Cross-platform.** Desktop (macOS Metal, Windows x64, Linux Flatpak) and mobile
  (Android, iOS) hosts with touch controls and game-controller support.

## Supported platforms

| Platform | Status |
|---|---|
| macOS (Apple Silicon, Metal) | Primary target; full speed |
| Windows x64 | Native build (VS 2022 clang-cl) |
| Linux (Flatpak) | aarch64 and x86_64 |
| Android (ARM64) | APK with touch + controller input |
| iOS (ARM64) | IPA, requires personal signing |

See [`ports/README.md`](ports/README.md) for the mobile, Windows, and Linux
builds.

## Requirements

A C11/C++23 toolchain, CMake, Ninja, and pkg-config. The default LLVM backend
also needs LLVM 19 or 20; the portable C backend is always available with
`BACKEND=c`.

### macOS

```sh
brew install cmake ninja pkg-config
```

Xcode command line tools are also required (AppleClang 14.0.3+).

### Linux (Ubuntu/Debian)

```sh
sudo apt-get install -y ninja-build build-essential pkg-config cmake \
  libevdev-dev libudev-dev libgtk-3-dev libsystemd-dev \
  libbluetooth-dev libasound2-dev libpulse-dev libgl1-mesa-dev \
  libxrandr-dev libxi-dev
```

### Windows

Visual Studio 2022 with the "Desktop development with C++" workload, plus CMake
and Ninja on `PATH`. The shipped Windows package is cross-compiled; see
[`ports/README.md`](ports/README.md).

## Build and run

Clone with submodules, then build — the Turbulence vendor patches are applied
automatically the first time you build:

```sh
git clone --recurse-submodules https://github.com/BittahBandit1/Turbulence.git
cd Turbulence
make run ISO=/path/to/Sonic\ Riders\ \(USA\).iso
```

`make run` builds DolRecomp and ModernGekko, applies the patches, extracts your
ISO, recompiles `main.dol` and `_Main.rel`, and launches the game. The ISO is
only needed the first time; afterwards select the extracted game by slug:

```sh
make run GAME=Sonic-Riders-USA-En-Ja-Fr-De-Es-It
```

If you cloned without `--recurse-submodules`, `make` fetches them on first run.
To apply the vendor patches manually: `make patches` (or
`sh scripts/apply-patches.sh`).

Useful targets:

| Target | Description |
|---|---|
| `make check` | Validate the toolchain and architecture |
| `make tools` | Build DolRecomp and ModernGekko |
| `make run ISO=…` | Extract, recompile, and launch |
| `make recompile GAME=… BACKEND=c` | Portable C backend |
| `make patches` | Apply the Turbulence vendor patches |
| `make save-patches` | Regenerate `patches/` from edited submodules |

### Variables

| Variable | Default | Description |
|---|---|---|
| `ISO` | *(none)* | Path to your ISO (first run only) |
| `GAME` | *(none)* | Select an already-extracted game by slug |
| `BACKEND` | `llvm` | Generated-code backend: `llvm` or `c` |
| `LLVM_DIR` | *(discovered)* | Path to LLVM 19/20's CMake package |
| `TOOLCHAIN` | `auto` | `auto`, `clang`, `gcc`, or `msvc` for the module |
| `RUN_ARGS` | *(empty)* | Extra flags forwarded to the runner (`--graphics Vulkan`, `--headless`) |
| `CMAKE_BUILD_TYPE` | `Release` | Build type for the tools |

### Controller input

GameCube bindings come from `Config/GCPadNew.ini` in ModernGekko's user
directory (`~/.local/share/moderngekko/Config/` by default). A working example is
provided in [`config/GCPadNew.ini`](config/GCPadNew.ini) and copied into place by
[`launch_sonic_riders.sh`](launch_sonic_riders.sh). Dolphin's ini format applies.

The options menu (R1 / Tab) switches button-prompt artwork between GameCube, Xbox,
and PlayStation; prompts also follow the last-used controller automatically.

### macOS launcher

A native SwiftUI launcher lives in [`launcher/`](launcher). It locates the runtime,
the compiled game module, and the extracted disc, then launches the game with the
correct arguments — no shell required. Build it with:

```sh
make launcher            # -> build/release/Turbulence.app
```

If the launcher is run outside the source tree, point it at the repo with the
`TURBULENCE_ROOT` environment variable.

## Project layout

```
Makefile               top-level build driver
launcher/              Swift macOS launcher app
patches/               Turbulence's changes to the upstream submodules
config/                example controller / graphics configs
src/                   native runtime sources (options, prompts, pacing)
ports/                 Android, iOS, Windows, and Linux hosts
scripts/               build, patch, and validation scripts
docs/                  status notes and platform handoffs
lib/DolRecomp          (submodule) static recompiler
lib/ModernGekko        (submodule) Dolphin-derived runtime
```

## How the patches work

DolRecomp and ModernGekko are tracked as upstream git submodules, and Turbulence's
modifications are kept as git patches in `patches/` rather than as forked copies.
`make` applies them on top of a fresh submodule checkout (idempotently), so the
submodules themselves never carry uncommitted local changes. After editing a
submodule, capture the changes with `make save-patches`.

## License

Turbulence is distributed under the [GNU GPL v3 (or later)](LICENSE), matching its
ModernGekko/Dolphin-derived runtime. DolRecomp and ModernGekko carry their own
upstream licenses (`lib/DolRecomp/LICENSE`, `lib/ModernGekko/LICENSE`). The
Android and iOS hosts are adapted from the Crazy Taxi ReFare project (GPL-3.0; see
`ports/COPYING`).

*Sonic Riders* and all related artwork are © SEGA. No disc images, extracted game
data, or keys are included.

## Contributors

- **BittahBandit1** — project author and maintainer
- **Claude** (Anthropic) — development
- **DeepSeek** — development

## Acknowledgments

- [DolRecomp](https://github.com/ExpansionPak/DolRecomp) and
  [ModernGekko](https://github.com/ExpansionPak/ModernGekko) by ExpansionPak
- [Dolphin](https://dolphin-emu.org/) — the runtime's video/audio/HLE core
- Crazy Taxi ReFare — source of the mobile hosts
